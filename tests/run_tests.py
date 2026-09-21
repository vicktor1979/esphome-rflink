#!/usr/bin/env python3
"""Offline, host-only tests. Does NOT replace an ESPHome firmware build or RF tests."""
from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('stage_sources', ROOT / 'components/rflink/stage_sources.py')
staging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(staging)


def main():
    expected = json.loads((ROOT / 'UPSTREAM_SHA256.json').read_text())
    actual = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in sorted((ROOT / 'RFLink/Plugins').iterdir()) if p.is_file()}
    assert actual == expected, 'Original plugin directory changed (contents or file set).'
    assert len(actual) == 54
    assert len(staging.discover(ROOT)) == 48
    assert len(staging.select_plugins(ROOT, 'configured')) == 47
    assert len(staging.select_plugins(ROOT, 'all')) == 48
    assert staging.select_plugins(ROOT, [61]) == [1, 61]
    try:
        staging.select_plugins(ROOT, [999])
    except ValueError:
        pass
    else:
        raise AssertionError('Missing plugin must fail validation.')
    print('PASS: all 54 source hashes; source discovery and selections.', flush=True)
    compiler = shutil.which('g++')
    if not compiler:
        raise SystemExit('g++ is required for host tests; source hash checks passed.')
    for selection in ([61], [34, 40, 61], 'configured', 'all'):
        with tempfile.TemporaryDirectory(prefix='rflink-test-') as directory:
            build = Path(directory)
            ids = staging.stage(ROOT, build / 'src/rflink_vendor', selection)
            generated = build / 'src/rflink_vendor'
            assert not list(generated.rglob('*.c')), 'Do not compile plugin fragments as C.'
            for source in (ROOT / 'RFLink/Plugins').glob('*.c'):
                assert (generated / 'Plugins' / (source.name + '.inc')).read_bytes() == source.read_bytes()
            registry = (generated / 'registry.inc').read_text()
            assert 'PLUGIN_TX_' not in registry
            assert registry.index('#define PLUGIN_061') < registry.index('#include "Plugins/Plugin_001.c.inc"')
            executable = build / 'host_test'
            command = [compiler, '-std=c++17', '-I' + str(ROOT / 'tests/stubs'),
                       '-I' + str(build / 'src'), '-I' + str(ROOT / 'components/rflink'),
                       str(ROOT / 'components/rflink/rflink_engine.cpp'),
                       str(ROOT / 'tests/host_test.cpp'), '-o', str(executable)]
            subprocess.run(command, check=True)
            result = subprocess.run([str(executable)], capture_output=True, text=True, check=True)
            data = json.loads(result.stdout.strip())
            assert data['NAME'] == 'EV1527' and data['ID'] == '01fac2'
            assert data['SWITCH'] == '08' and data['CMD'] == 'ON'
            print(f'Selection {selection}: {len(ids)} RX plugins.', flush=True)
            print(result.stdout, end='')
            print(result.stderr, end='')
    print('Host tests passed. ESP8266/ESP32 firmware build and real-radio tests have NOT been run here.')

if __name__ == '__main__':
    main()
