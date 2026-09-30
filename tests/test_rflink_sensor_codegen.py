#!/usr/bin/env python3
"""Regression for enum defaults using real ESPHome validation/code generation.
Run with an interpreter that has ESPHome 2026.9.0 and PyYAML installed.
This generates code only; it does not download/build the target toolchain.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
import yaml

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='rflink-sensor-codegen-') as directory:
    temp = Path(directory)
    config = yaml.safe_load((ROOT/'tests/compile-rflink-sensor.yaml').read_text())
    config['esphome']['build_path'] = str(temp/'build')
    config['external_components'][0]['source']['path'] = str(ROOT/'components')
    config['sensor'] = []
    for field in ['TEMP', 'HUM', 'BARO', 'WINCHL', 'WINTMP']:
        config['sensor'].append(dict(platform='rflink_sensor', id='test_'+field.lower(),
            name='Test '+field, protocol='Mebus', rf_id='e501', field=field, rflink_id='rf_bridge'))
    # Explicit user overrides must pass through unchanged.
    for label, value in [('total','total'), ('none','')]:
        config['sensor'].append(dict(platform='rflink_sensor', id='test_'+label,
            name='Test '+label, protocol='Mebus', rf_id='e501', field='TEMP',
            state_class=value, rflink_id='rf_bridge'))
    config['sensor'].append(dict(platform='rflink_sensor', id='test_default',
        name='Default field', protocol='Mebus', rf_id='e501', rflink_id='rf_bridge'))
    config_path = temp/'config.yaml'
    config_path.write_text(yaml.safe_dump(config, allow_unicode=True, sort_keys=False))
    subprocess.run([sys.executable, '-m', 'esphome', 'compile', '--only-generate', str(config_path)], check=True)
    cpp = (temp/'build/src/main.cpp').read_text()
    for name in ['temp','hum','baro','winchl','wintmp','default']:
        assert f'test_{name}->set_state_class(sensor::STATE_CLASS_MEASUREMENT);' in cpp, name
    assert 'test_total->set_state_class(sensor::STATE_CLASS_TOTAL);' in cpp
    assert 'test_none->set_state_class(sensor::STATE_CLASS_NONE);' in cpp
    assert 'set_state_class("' not in cpp
    assert 'test_default->set_field(rflink_data::TEMP);' in cpp
    assert 'test_temp->set_accuracy_decimals(1);' in cpp
    assert 'test_hum->set_accuracy_decimals(0);' in cpp
    print('PASS: 6 automatic StateClass enums, explicit total/none preserved, no string-valued state_class, field and accuracy defaults.')
