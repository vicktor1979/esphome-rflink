#!/usr/bin/env python3
"""HOST tests only. Run with the full, already updated v0.1.5 RFLink repository.
python3 tests/rx_gate/run_tests.py --repo /path/to/esphome-rflink --out /tmp/rxgate-test
Requires Linux, g++, Python 3 and PyYAML. No hardware or ESPHome target compiler.
"""
from __future__ import annotations
import argparse, hashlib, importlib.util, json, os, re, shutil, subprocess, tempfile
from pathlib import Path
import yaml
HERE=Path(__file__).resolve().parent
PATCH=HERE.parents[1]
class Loader(yaml.SafeLoader): pass
def mapping(loader,node,deep=False):
    out={}
    for k,v in node.value:
        key=loader.construct_object(k,deep=deep)
        if key in out: raise ValueError(f'Duplicate YAML key: {key}')
        out[key]=loader.construct_object(v,deep=deep)
    return out
Loader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG,mapping)
for tag in ['!lambda','!secret','!include']:
    Loader.add_constructor(tag,lambda l,n:l.construct_scalar(n))
def load(path): return yaml.load(path.read_text(encoding='utf-8'),Loader=Loader)
def translate(body,variables):
    for key,val in variables.items():body=body.replace('${'+key+'}',str(val))
    if '${' in body:raise ValueError('Unresolved substitution')
    return re.sub(r'id\((\w+)\)',r'\1',body)
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--repo',type=Path,required=True)
    ap.add_argument('--out',type=Path,default=Path('/tmp/rflink-rxgate-results'))
    args=ap.parse_args();repo=args.repo.resolve();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    report=[]
    def passed(s):print('PASS: '+s,flush=True);report.append('PASS: '+s)
    config=load(repo/'examples/rflink.yaml')
    assert config['external_components'][0]['components']==['rflink','remote_receiver','rflink_remote']
    assert config['remote_receiver']['capture_enabled'] is False
    assert config['remote_receiver']['high_frequency'] is False
    assert config['rflink']['rx_plugins']=='all'
    assert config['remote_receiver']['pin']['number']=='GPIO5'
    assert config['remote_receiver']['filter']=='100us' and config['remote_receiver']['idle']=='5ms'
    assert config['remote_receiver']['buffer_size']=='1200b'
    assert 'dump' not in config['remote_receiver'] and 'mqtt' not in config
    ids=[]
    for doc in [config,load(repo/'packages/rflink-ha-data-only.yaml')]:
        for section in ['globals','event','binary_sensor','sensor','text_sensor','switch','script']:
            ids.extend(i['id'] for i in doc.get(section,[]) if 'id' in i)
    assert len(ids)==len(set(ids))
    passed('YAML parses with no duplicate keys/IDs; all RX, current data package and stable RX timings retained.')
    compile((repo/'components/remote_receiver/__init__.py').read_text(), '__init__.py','exec')
    passed('External receiver Python syntax compiles (NOT ESPHome schema validation).')
    hashes=json.loads((repo/'UPSTREAM_SHA256.json').read_text())
    assert len(hashes)==54
    for rel,digest in hashes.items(): assert hashlib.sha256((repo/rel).read_bytes()).hexdigest()==digest,rel
    passed('54 upstream RFLink plugin/config/old files match original manifest.')
    spec=importlib.util.spec_from_file_location('rxgate_stage',repo/'components/rflink/stage_sources.py')
    stage=importlib.util.module_from_spec(spec);spec.loader.exec_module(stage)
    with tempfile.TemporaryDirectory(prefix='rflink-rxgate-') as t:
        t=Path(t);src=t/'src';src.mkdir()
        selected=stage.stage(repo,src/'rflink_vendor','all');assert len(selected)==48
        for f in (repo/'RFLink/Plugins').glob('Plugin_*.c'):
            assert (src/'rflink_vendor/Plugins'/(f.name+'.inc')).read_bytes()==f.read_bytes()
        passed('All 48 RX plugins staged; generated plugin text byte-identical.')
        funcs={
            'auto_step':config['interval'][0]['then'][-1]['lambda'],
            'manual_off':config['switch'][0]['turn_off_action'][0]['lambda'],
            'ota_begin':config['ota'][0]['on_begin']['then'][0]['lambda'],
            'ota_error':config['ota'][0]['on_error']['then'][0]['lambda'],
            'wifi_lost':config['wifi']['on_disconnect']['then'][0]['lambda'],
        }
        (t/'yaml_gate_functions.inc').write_text('\n'.join('void '+name+'(){\n'+translate(body,config['substitutions'])+'\n}' for name,body in funcs.items()))
        cmd=['g++','-std=gnu++20','-DESP8266','-DUSE_ESP8266','-Wall','-Wextra','-g',
             '-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DRFLINK_TEST_STRICT_PROGMEM',
             '-I'+str(HERE/'stubs'),'-I'+str(src),'-I'+str(t),'-I'+str(repo/'components/rflink'),
             '-I'+str(repo/'components/remote_receiver'),
             str(repo/'components/remote_receiver/remote_receiver.cpp'),
             str(repo/'components/rflink/rflink.cpp'),str(repo/'components/rflink/rflink_engine.cpp'),
             str(HERE/'test_capture.cpp'),'-o',str(t/'test_capture')]
        env=dict(os.environ,LC_ALL='C',ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        result=subprocess.run(cmd,env=env,text=True,capture_output=True,timeout=90)
        (out/'capture_compile.log').write_text('$ '+' '.join(cmd)+'\n'+result.stdout+result.stderr+f'\nEXIT {result.returncode}\n')
        if result.returncode:raise RuntimeError(result.stderr)
        passed('Actual modified receiver + actual RFLink v0.1.5 bridge + all 48 RX plugins compile under host GNU C++20.')
        result=subprocess.run([str(t/'test_capture')],env=env,text=True,capture_output=True,timeout=30)
        (out/'capture_run.log').write_text(result.stdout+result.stderr+f'\nEXIT {result.returncode}\n')
        if result.returncode:raise RuntimeError(result.stdout+result.stderr)
        print(result.stdout,flush=True);report.extend(result.stdout.strip().splitlines())
        # Reuse the actual receiver and engine; now drive GPIO edges while servicing
        # receiver.loop() at a simulated 16 ms period, not only at frame boundaries.
        cmd2=[str(HERE/'test_scheduled_capture.cpp') if item==str(HERE/'test_capture.cpp') else item for item in cmd]
        cmd2[-1]=str(t/'test_scheduled_capture')
        result=subprocess.run(cmd2,env=env,text=True,capture_output=True,timeout=90)
        (out/'scheduled_compile.log').write_text('$ '+' '.join(cmd2)+'\n'+result.stdout+result.stderr+f'\nEXIT {result.returncode}\n')
        if result.returncode:raise RuntimeError(result.stderr)
        result=subprocess.run([str(t/'test_scheduled_capture')],env=env,text=True,capture_output=True,timeout=30)
        (out/'scheduled_run.log').write_text(result.stdout+result.stderr+f'\nEXIT {result.returncode}\n')
        if result.returncode:raise RuntimeError(result.stdout+result.stderr)
        print(result.stdout,flush=True);report.extend(result.stdout.strip().splitlines())
    for rel,digest in hashes.items(): assert hashlib.sha256((repo/rel).read_bytes()).hexdigest()==digest,rel
    passed('Original plugin contents unchanged after tests.')
    report.extend(['LIMIT: GPIO/interrupts, ESPHome base classes, clocks and Wi-Fi/API flags are simulated test doubles.',
                   'LIMIT: No actual ESPHome schema/code generation, Xtensa firmware build, OTA or Wi-Fi/RF hardware test.',
                   'LIMIT: Passing host tests does NOT establish the cause of the Wi-Fi failure or hardware stability.',
                   'KNOWN: Legacy plugin compiler warnings are retained, not suppressed.'])
    (out/'SUMMARY.txt').write_text('\n'.join(report)+'\n')
if __name__=='__main__':main()
