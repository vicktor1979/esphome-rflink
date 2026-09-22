#!/usr/bin/env python3
"""Offline HOST C++20 tests. Not ESPHome generation, Xtensa build or hardware tests.
Run: python3 tests/all_plugins/test_all_plugins.py --repo . --out /tmp/rflink-tests
Requires g++, Python 3.10+, PyYAML, Linux (mmap guard). Test stubs NEVER enter firmware.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import yaml
from gesture_integration_parts import PREAMBLE, TESTBODY

HERE=Path(__file__).resolve().parent
class Loader(yaml.SafeLoader): pass
def unique(loader,node,deep=False):
    result={}
    for kn,vn in node.value:
        key=loader.construct_object(kn,deep=deep)
        if key in result: raise ValueError(f'Duplicate YAML key: {key}')
        result[key]=loader.construct_object(vn,deep=deep)
    return result
Loader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG,unique)
for tag in ['!lambda','!secret','!include']:
    Loader.add_constructor(tag,lambda l,n:l.construct_scalar(n))
def load(p): return yaml.load(p.read_text(encoding='utf-8'),Loader=Loader)
def translate(body,variables):
    for k,v in variables.items():body=body.replace('${'+k+'}',str(v))
    assert '${' not in body,body
    return re.sub(r'id\((\w+)\)',r'\1',body)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo',type=Path,default=HERE.parents[1])
    parser.add_argument('--out',type=Path,default=Path('/tmp/rflink-all-host-results'))
    args=parser.parse_args();repo=args.repo.resolve();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    assert shutil.which('g++'), 'g++ not installed'
    report=[]
    def passed(s):print('PASS: '+s,flush=True);report.append('PASS: '+s)
    def run(cmd,name,expected=0):
        env=dict(os.environ,LC_ALL='C',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',ASAN_OPTIONS='detect_leaks=1')
        result=subprocess.run([str(s) for s in cmd],text=True,capture_output=True,env=env,timeout=90)
        (out/(name+'.log')).write_text('$ '+' '.join(map(str,cmd))+'\n'+result.stdout+result.stderr+f'\nEXIT {result.returncode}\n')
        if expected==0 and result.returncode!=0:
            print(result.stdout+result.stderr,file=sys.stderr);raise RuntimeError(f'{name}: exit {result.returncode}')
        if expected!=0:assert result.returncode!=0,name
        return result
    spec=importlib.util.spec_from_file_location('stage_sources',repo/'components/rflink/stage_sources.py')
    stage=importlib.util.module_from_spec(spec);spec.loader.exec_module(stage)
    hashes=json.loads((repo/'UPSTREAM_SHA256.json').read_text())
    for rel,digest in hashes.items():assert hashlib.sha256((repo/rel).read_bytes()).hexdigest()==digest,rel
    assert len(hashes)==54
    passed('54 original plugin/config/old-file SHA256 digests match the original manifest.')
    available=stage.discover(repo);all_ids=stage.select_plugins(repo,'all');configured=stage.select_plugins(repo,'configured')
    assert len(all_ids)==48 and len(configured)==47 and set(all_ids)-set(configured)=={83}
    assert stage.select_plugins(repo,[61])==[1,61]
    assert stage.select_plugins(repo,[34,40,61])==[1,34,40,61]
    try: stage.select_plugins(repo,[999])
    except ValueError: pass
    else: raise AssertionError('Missing plugin did not produce an error')
    passed('all=48; configured=47; the difference is Plugin_083; explicit selection and missing-ID validation.')
    main_yaml=load(repo/'rflink-all-plugins-proba.yaml')
    gesture=load(repo/'packages/rflink-ha-gestures.yaml');fields=load(repo/'packages/rflink-ha-all-data.yaml')
    assert main_yaml['rflink']['rx_plugins']=='all'
    assert main_yaml['remote_receiver']['pin']['number']=='GPIO5'
    assert main_yaml['remote_receiver']['buffer_size']=='1000b'
    assert main_yaml['remote_receiver']['filter']=='100us' and main_yaml['remote_receiver']['idle']=='5ms'
    assert 'mqtt' not in main_yaml and 'dump' not in main_yaml['remote_receiver']
    ids=[]
    for doc in [main_yaml,gesture,fields]:
        for section in ['globals','event','binary_sensor','sensor','text_sensor','switch','script']:
            ids.extend(i['id'] for i in doc.get(section,[]) if 'id' in i)
    assert len(ids)==len(set(ids))
    passed('YAML unique keys/IDs; all RX selection; receiver timings, API gate, field and gesture packages retained.')
    with tempfile.TemporaryDirectory(prefix='rflink-all-') as temp:
        temp=Path(temp);src=temp/'src';vendor=src/'rflink_vendor'
        component=repo/'components/rflink'
        common=['g++','-std=gnu++20','-DESP8266','-DUSE_ESP8266','-Wall','-I'+str(HERE/'stubs'),'-I'+str(src),'-I'+str(component)]
        syntax=common+['-fno-exceptions','-fno-rtti','-fsyntax-only']
        stage.stage(repo,vendor,'all')
        original_registry=(vendor/'registry.inc').read_text()
        unpatched=re.sub(r'// Plugin_083 read-only legacy pointer compatibility.*?\n#pragma pop_macro\("PSTR"\)', '#include "Plugins/Plugin_083.c.inc"', original_registry, flags=re.S)
        assert unpatched!=original_registry
        (vendor/'registry.inc').write_text(unpatched)
        bad=run(syntax+[component/'rflink_engine.cpp'],'expected_unpatched_083_failure',expected=1)
        assert 'invalid conversion' in bad.stderr and 'Plugin_083.c.inc' in bad.stderr
        (vendor/'registry.inc').write_text(original_registry)
        passed('Unpatched all-plugin build fails with real const PSTR typing (Plugin_083); regression reproduced.')
        for label,selection in [('all','all'),('configured','configured'),('ev1527',[61]),('weather_ev1527',[34,40,61])]:
            chosen=stage.stage(repo,vendor,selection)
            run(syntax+[component/'rflink_engine.cpp'],'compile_'+label)
            for n,p in available.items():assert (vendor/'Plugins'/(p.name+'.inc')).read_bytes()==p.read_bytes()
            manifest=json.loads((vendor/'manifest.json').read_text());assert manifest['rx_plugins']==chosen and manifest['tx_enabled'] is False
            assert not list(vendor.rglob('*.c'))
            passed(f'HOST C++20 syntax build: {label}, {len(chosen)} RX plugins; byte-identical staged plugins; TX disabled.')
        for n in all_ids:
            stage.stage(repo,vendor,[n]);run(syntax+[component/'rflink_engine.cpp'],f'compile_individual_{n:03}')
        passed('All 48 per-plugin selections compile individually (001 automatically present).')
        # Check the fallback wrapper used by Arduino cores without PSTRN.
        stage.stage(repo,vendor,'all')
        fallback=temp/'fallback.cpp'
        fallback.write_text('#include <Arduino.h>\n#undef PSTRN\n#undef PSTR_ALIGN\n#undef PSTR\n#define PSTR(s) ([]() -> const char * { static const char x[]=(s); return x; }())\n#include "rflink_engine.cpp"\n#include <type_traits>\nvoid verify(){ auto p=PSTR("restored"); static_assert(std::is_same_v<decltype(p),const char*>); }\n')
        run(syntax+[fallback],'compile_fallback_macro')
        passed('Scoped PSTR fallback compiles; framework const PSTR type is restored afterwards.')
        # No PIE avoids sporadic sanitizer address-space collisions in container hosts.
        runtime=common+['-g','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DRFLINK_TEST_STRICT_PROGMEM']
        run(runtime+['-Wextra','-Werror',HERE/'gesture_test.cpp','-o',temp/'unit'],'gesture_state_compile')
        unit=run([temp/'unit'],'gesture_state_run');passed(unit.stdout.strip())
        setup=translate(gesture['esphome']['on_boot']['then'][0]['lambda'],gesture['substitutions'])
        tick=translate(gesture['interval'][0]['then'][0]['lambda'],gesture['substitutions'])
        types=','.join('"'+t+'"' for t in gesture['event'][0]['event_types'])
        assert gesture['event'][0]['event_types']==gesture['event'][1]['event_types']
        code=PREAMBLE+'\nvoid setup_gestures(){\nrf_gesture_event_1.allowed={'+types+'};\nrf_gesture_event_2.allowed=rf_gesture_event_1.allowed;\n'+setup+'\n}\nvoid tick_gestures(){\n'+tick+'\n}\n'+TESTBODY
        integration=temp/'integration.cpp';integration.write_text(code)
        for label,selection in [('ev1527',[61]),('weather_ev1527',[34,40,61]),('configured','configured'),('all','all')]:
            stage.stage(repo,vendor,selection)
            run(runtime+[component/'rflink.cpp',component/'rflink_engine.cpp',integration,'-o',temp/'integration'],'gesture_integration_compile_'+label)
            result=run([temp/'integration'],'gesture_integration_run_'+label)
            passed(f'Real bridge + gesture YAML lambda execution, guarded const flash, ASan/UBSan: {label}. '+result.stdout.strip().splitlines()[-1])
        for label,selection in [('brel',[83]),('all','all')]:
            stage.stage(repo,vendor,selection)
            run(runtime+[HERE/'brel_test.cpp','-o',temp/'brel'],'brel_compile_'+label)
            result=run([temp/'brel'],'brel_run_'+label)
            passed('Brel four command paths, flash reads and PSTR restoration: '+label)
        stage.stage(repo,vendor,'all')
        run(runtime+[repo/'tests/formatter_test.cpp','-o',temp/'formatter'],'formatter_compile_all')
        result=run([temp/'formatter'],'formatter_run_all');passed(result.stdout.strip())
    for rel,digest in hashes.items():assert hashlib.sha256((repo/rel).read_bytes()).hexdigest()==digest,rel
    passed('Original 54 plugin/config/old files remain unchanged after every test.')
    report += ['LIMIT: Host compiler and Arduino/ESPHome test doubles; NOT a full ESPHome/Xtensa firmware build.',
               'LIMIT: No real RF, Wi-Fi, API, OTA, flash size, target RAM or long-term stability test was possible.',
               'KNOWN: Original Plugin_037 printf format warning is not suppressed.',
               'KNOWN: Full legacy decoder sets can emit repeated EV1527 JSON; gesture frame logic passes independently.',
               'KNOWN: Original Plugin_083 emits duplicate NAME fields for its command; source semantics unchanged.']
    (out/'SUMMARY.txt').write_text('\n'.join(report)+'\n')
    print('\n'.join(report[-5:]),flush=True)
if __name__=='__main__':main()
