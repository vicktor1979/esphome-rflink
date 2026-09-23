"""Stage byte-identical RFLink sources for inclusion in ONE C++ translation unit.

The RFLink/Plugins directory in the repository is read-only to this script.
Only build copies gain a .inc suffix, preventing a second, incorrect C compile.
"""
from pathlib import Path
import hashlib
import json
import re

PLUGIN_NAME = re.compile(r"Plugin_(\d{3})\.c$")


def _without_comments(text: str) -> str:
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)


def discover(repo: Path) -> dict[int, Path]:
    directory = repo / "RFLink" / "Plugins"
    if not directory.is_dir():
        raise ValueError(f"Missing {directory}; upload RFLink/ together with components/.")
    return {int(m[1]): p for p in sorted(directory.iterdir())
            if p.is_file() and (m := PLUGIN_NAME.fullmatch(p.name))}


def extension_manifest(repo: Path) -> dict:
    path = repo / "RFLink/Extensions/manifest.json"
    if not path.is_file():
        raise ValueError("plugin_profile: extended requires RFLink/Extensions/manifest.json and its files.")
    content = json.loads(path.read_text())
    if content.get("version") != "0.1.7":
        raise ValueError("Unsupported extensions manifest version; update components/rflink and RFLink/Extensions together.")
    root = (repo / "RFLink/Extensions").resolve()
    for group in ("new_plugins", "overrides"):
        for entry in content[group].values():
            file = (root / entry["path"]).resolve()
            if root not in file.parents or not file.is_file():
                raise ValueError(f"Missing/unsafe extension path: {file}")
    return content


def select_plugins(repo: Path, selection="configured", profile="legacy") -> list[int]:
    if profile not in ("legacy", "extended"):
        raise ValueError("plugin_profile must be legacy or extended")
    available = discover(repo)
    if profile == "extended":
        manifest = extension_manifest(repo)
        duplicates = set(map(int, manifest["new_plugins"])) & set(available)
        if duplicates:
            raise ValueError(f"Original/extension plugin ID conflict: {sorted(duplicates)}")
        available.update({int(n): repo/"RFLink/Extensions"/v["path"] for n,v in manifest["new_plugins"].items()})
    if selection == "configured":
        text = _without_comments((repo / "RFLink/Plugins/_Plugin_Config_01.h").read_text())
        chosen = {int(n) for n in re.findall(r"^\s*#\s*define\s+PLUGIN_(\d{3})\b", text, re.M)}
    elif selection == "all":
        chosen = set(available)
    elif isinstance(selection, (list, tuple)):
        chosen = {int(n) for n in selection}
    else:
        raise ValueError("rx_plugins must be 'configured', 'all', or a list of plugin IDs.")
    chosen.add(1)  # Original preprocessor/translation plugin must be first.
    missing = chosen - set(available)
    if missing:
        raise ValueError("Missing plugin source(s): " + ", ".join(f"{n:03d}" for n in sorted(missing)))
    if not chosen or len(chosen) > 255:
        raise ValueError("This RFLink ABI supports at most 255 receiver plugins.")
    return sorted(chosen)


def write_changed(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def stage(repo: Path, destination: Path, selection="configured", profile="legacy") -> list[int]:
    repo, destination = Path(repo), Path(destination)
    ids = select_plugins(repo, selection, profile)
    available = discover(repo)
    extra = extension_manifest(repo) if profile == "extended" else {"new_plugins": {}, "overrides": {}}
    extra_ids = {int(n) for n in extra["new_plugins"]}
    legacy_ids = [n for n in ids if n not in extra_ids]
    if profile == "extended":
        for n, entry in extra["overrides"].items():
            base = available.get(int(n))
            if base is None or hashlib.sha256(base.read_bytes()).hexdigest() != entry["base_sha256"]:
                raise ValueError(f"Original Plugin_{int(n):03d}.c differs from audited base; refusing an implicit override.")
        for source in sorted((repo/"RFLink/Extensions").rglob("*")):
            if source.is_file() and source.suffix in (".inc", ".c"):
                relative = source.relative_to(repo/"RFLink/Extensions")
                # Overrides use ../4_Display.h relative to their staged location.
                target = destination / ("Overrides" if relative.parts[0] == "overrides" else "Extensions")
                if relative.parts[0] == "overrides":
                    target = target / (source.name + ".inc")
                else:
                    target = target / relative
                write_changed(target, source.read_bytes())
    # Do not put .c files in src/: these plugins are C++ fragments, not C units.
    for plugin_id, source in available.items():
        write_changed(destination / "Plugins" / (source.name + ".inc"), source.read_bytes())
    for name in ["1_Radio.h", "2_Signal.h", "3_Serial.h", "4_Display.h", "5_Plugin.h", "7_Utils.h"]:
        write_changed(destination / name, (repo / "RFLink" / name).read_bytes())
    write_changed(destination / "7_Utils.cpp.inc", (repo / "RFLink/7_Utils.cpp").read_bytes())
    registry = ["// Generated file. Original RFLink/Plugins files have NOT been edited."]
    # Older Arduino binary constants may not exist in every Arduino core.
    binary_literals = sorted({token for source in available.values()
                              for token in re.findall(r"\bB[01]{1,8}\b", source.read_text())})
    for token in binary_literals:
        registry += [f"#ifndef {token}", f"#define {token} 0b{token[1:]}", "#endif"]
    registry += [f"#define RFLINK_PROFILE_EXTENDED {1 if profile == 'extended' else 0}",
                 f"#define RFLINK_TOTAL_PLUGINS {len(ids)}",
                 f'#define RFLINK_PLUGIN_PROFILE "{profile}"']
    registry += [f"#define PLUGIN_{n:03d}" for n in legacy_ids]
    for n in legacy_ids:
        if str(n) in extra["overrides"]:
            registry += [f'#include "Overrides/Plugin_{n:03d}.c.inc"']
            continue
        if n == 83:
            # Audited legacy Brel/Dooya RX uses char * for four PSTR results,
            # but ONLY reads them through display_Name(). Keep both the original
            # source and the build copy byte-identical. Restore the framework
            # macro immediately after this one include; never use -fpermissive.
            registry += [
                "// Plugin_083 read-only legacy pointer compatibility (v0.1.5).",
                '#pragma push_macro("PSTR")',
                "#undef PSTR",
                "#if defined(PSTRN) && defined(PSTR_ALIGN)",
                "#define PSTR(s) (const_cast<char *>(PSTRN(s, PSTR_ALIGN)))",
                "#else",
                "#define PSTR(s) ([]() -> char * { alignas(4) static const char text[] PROGMEM = (s); return const_cast<char *>(text); }())",
                "#endif",
            ]
        registry += [f'#include "Plugins/Plugin_{n:03d}.c.inc"']
        if n == 83:
            registry += ['#pragma pop_macro("PSTR")']
    registry += ["struct PluginEntry { unsigned id; bool (*decode)(byte, char *); };",
                 "static const PluginEntry RX_PLUGINS[] = {"]
    registry += [f"  {{{n}, &Plugin_{n:03d}}}," for n in legacy_ids]
    registry += ["};", ""]
    if profile == "extended":
        registry += ['#include "Extensions/extended_common.inc"']
        active_ext = [n for n in ids if n in extra_ids]
        for n in active_ext:
            registry += [f'#include "Extensions/decoders/Plugin_{n:03d}.inc"']
        if 72 in ids:
            registry += ['#include "Extensions/decoders/Plugin_072_raw.inc"']
        registry += ["struct ExtensionEntry { unsigned id; bool (*decode)(const rf_ext::Pulses &); };",
                     "static const ExtensionEntry EXT_PLUGINS[] = {"]
        # Strong checksums/sync precede legacy heuristics; all original decoders retain their order.
        for n in active_ext:
            registry += [f"  {{{n}, &rf_ext::decode_{n:03d}}},"]
        if 72 in ids:
            registry += ["  {72, &rf_ext::decode_072_raw},"]
        registry += ["  {0, nullptr},", "};", ""]
    write_changed(destination / "registry.inc", "\n".join(registry).encode())
    info = {
        "rx_plugins": ids, "tx_enabled": False, "plugin_profile": profile,
        "legacy_rx_plugins": legacy_ids, "new_rx_plugins": sorted(set(ids) & extra_ids),
        "active_overrides": [int(n) for n in extra["overrides"] if int(n) in ids],
        "extension_sha256": {str(p.relative_to(repo)): hashlib.sha256(p.read_bytes()).hexdigest()
                             for p in sorted((repo/"RFLink/Extensions").rglob("*")) if p.is_file()} if profile == "extended" else {},
        "source_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in sorted((repo / "RFLink/Plugins").iterdir()) if p.is_file()},
    }
    write_changed(destination / "manifest.json", (json.dumps(info, indent=2)+"\n").encode())
    return ids
