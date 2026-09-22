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


def select_plugins(repo: Path, selection="configured") -> list[int]:
    available = discover(repo)
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


def stage(repo: Path, destination: Path, selection="configured") -> list[int]:
    repo, destination = Path(repo), Path(destination)
    ids = select_plugins(repo, selection)
    available = discover(repo)
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
    registry += [f"#define PLUGIN_{n:03d}" for n in ids]
    for n in ids:
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
    registry += [f"  {{{n}, &Plugin_{n:03d}}}," for n in ids]
    registry += ["};", ""]
    write_changed(destination / "registry.inc", "\n".join(registry).encode())
    info = {
        "rx_plugins": ids, "tx_enabled": False,
        "source_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in sorted((repo / "RFLink/Plugins").iterdir()) if p.is_file()},
    }
    write_changed(destination / "manifest.json", (json.dumps(info, indent=2)+"\n").encode())
    return ids
