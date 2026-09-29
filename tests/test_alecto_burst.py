#!/usr/bin/env python3
"""v0.2.0.8 original Alecto path regressions (historical filename retained).
Optional first argument: timestamped signed raw rows. Host checks only.
"""
from pathlib import Path
import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('stage_alecto', ROOT / 'components/rflink/stage_sources.py')
stage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(stage)

with tempfile.TemporaryDirectory(prefix='rflink-alecto-') as tmp:
    build = Path(tmp)
    for profile in ('legacy', 'extended'):
        selected = stage.stage(ROOT, build / 'rflink_vendor',
                               'all' if profile == 'extended' else [30, 61, 254], profile)
        binary = build / ('test_' + profile)
        cmd = ['g++', '-std=gnu++20', '-O1', '-g', '-fno-pie', '-no-pie',
               '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
               '-I' + str(build), '-I' + str(ROOT / 'tests/remote_config/stubs'),
               '-I' + str(ROOT / 'components/rflink'),
               str(ROOT / 'components/rflink/rflink_engine.cpp'),
               str(ROOT / 'components/rflink/rflink.cpp'),
               str(ROOT / 'tests/alecto_burst_test.cpp'), '-o', str(binary)]
        subprocess.run(cmd, check=True)
        args = [str(binary)] + ([str(Path(sys.argv[1]).resolve())] if len(sys.argv) > 1 else [])
        result = subprocess.run(args, text=True, capture_output=True,
                                env=dict(os.environ, UBSAN_OPTIONS='halt_on_error=1'), timeout=45)
        if result.returncode:
            print(result.stdout); print(result.stderr, file=sys.stderr)
            raise SystemExit(result.returncode)
        print(profile.upper(), 'compiled plugins:', len(selected))
        for line in result.stdout.splitlines():
            if line.startswith(('PASS', 'REPLAY')):
                print(line)
    # Compile the actual auto-start/ESP8266 diagnostic branch too. Older host
    # receiver shims predate the backlog getters, so use the current header.
    auto_stubs = build / 'auto_stubs'
    shutil.copytree(ROOT / 'tests/rx_gate/stubs', auto_stubs)
    shutil.copy2(ROOT / 'components/remote_receiver/remote_receiver.h',
                 auto_stubs / 'esphome/components/remote_receiver/remote_receiver.h')
    for kind in ('sensor', 'text_sensor', 'binary_sensor'):
        rel = Path('esphome/components') / kind / (kind + '.h')
        shutil.copy2(ROOT / 'tests/remote_config/stubs' / rel, auto_stubs / rel)
    subprocess.run(['g++', '-std=gnu++20', '-DESP8266', '-DUSE_ESP8266',
                    '-DUSE_RFLINK_AUTO_START', '-DUSE_NETWORK', '-DUSE_API',
                    '-I' + str(auto_stubs), '-I' + str(ROOT / 'components/rflink'),
                    '-c', str(ROOT / 'components/rflink/rflink.cpp'),
                    '-o', str(build / 'autostart.o')], check=True)
    print('PASS ESP8266 auto-start and periodic diagnostic branch compile with host shims')
print('LIMIT: host tests only; no Xtensa firmware build, RF hardware, Wi-Fi/API/OTA timing test.')
