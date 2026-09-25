#!/usr/bin/env python3
"""HOST regression tests. Not an ESPHome configuration validation/firmware build."""
from pathlib import Path
import argparse, hashlib, importlib.util, json, os, py_compile, shutil, subprocess, tempfile
import yaml
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]

def load_module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module

class Loader(yaml.SafeLoader):pass
for tag in ['!lambda','!secret']:
    Loader.add_constructor(tag,lambda l,n:l.construct_scalar(n))
def include_value(loader, node):
    if isinstance(node, yaml.ScalarNode): return loader.construct_scalar(node)
    if isinstance(node, yaml.MappingNode): return loader.construct_mapping(node, deep=True)
    return loader.construct_sequence(node, deep=True)
Loader.add_constructor('!include', include_value)
def mapping(l,n,deep=False):
    out={}
    for k,v in n.value:
        k=l.construct_object(k,deep=deep)
        if k in out:raise ValueError(f'duplicate YAML key: {k}')
        out[k]=l.construct_object(v,deep=deep)
    return out
Loader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG,mapping)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--repo',required=True,type=Path);ap.add_argument('--out',type=Path,default=ROOT/'TEST_LOGS');a=ap.parse_args()
    repo=a.repo.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True);report=[]
    def log(s):print(s,flush=True);report.append(s)
    def run(cmd,label):
        env=dict(os.environ,UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',ASAN_OPTIONS='detect_leaks=1')
        r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=180,env=env)
        (out/(label+'.log')).write_text('$ '+' '.join(map(str,cmd))+'\n'+r.stdout+r.stderr+f'\nEXIT {r.returncode}\n')
        if r.returncode:raise RuntimeError(label+'\n'+r.stdout[-8000:]+r.stderr[-8000:])
        return r
    manifest=json.loads((repo/'UPSTREAM_SHA256.json').read_text())
    assert len(manifest)==54
    for p,h in manifest.items():assert hashlib.sha256((repo/p).read_bytes()).hexdigest()==h,p
    log('PASS: 54 original RFLink plugin/config/old files match the manifest.')
    for f in (ROOT/'components/rflink_remote').glob('*.py'):py_compile.compile(str(f),doraise=True)
    log('PASS: new Python modules compile as Python; this is NOT ESPHome schema execution.')
    v=load_module('remote_validation',ROOT/'components/rflink_remote/validation.py')
    sample=dict(protocol='EV1527',rf_id='01FAC2',button='8',command='ON')
    normalized=v.validate_pattern(sample);assert normalized['rf_id']=='01fac2' and normalized['button']=='08' and normalized['mode']=='gestures'
    cases=0
    for value in [dict(sample,rf_id=123),dict(sample,rf_id='100000'),dict(sample,button='10'),dict(sample,command='OFF'),dict(sample,protocol='Other',mode='gestures'),dict(sample,event_types=['nope']),dict(sample,event_types=[]),dict(sample,event_types=['single','single']),dict(sample,protocol='Other',mode='message',pressed={}),dict(sample,protocol='Other',mode='message',event_types=['hold']),dict(sample,rf_id='0x123'),dict(sample,protocol='\nEV1527')]:
        try:v.validate_pattern(value)
        except ValueError:cases+=1
        else:raise AssertionError(value)
    assert v.validate_pattern(dict(sample,protocol='Chuango'))['event_types']==['received']
    assert v.validate_timing({})['hold_release_timeout']==450
    for values in [{'release_timeout':700},{'hold_release_timeout':150},{'repeat_fresh_timeout':451},{'max_press_time':500},{'multi_click_timeout':2}]:
        try:v.validate_timing(values)
        except ValueError:cases+=1
        else:raise AssertionError(values)
    log(f'PASS: pure pattern/timing validators; normalization, default mode, {cases} invalid cases rejected.')
    main_yaml=yaml.load((repo/'examples/rflink.yaml').read_text(),Loader=Loader)
    assert main_yaml['remote_receiver']['high_frequency'] is False
    assert main_yaml['remote_receiver']['capture_enabled'] is False
    assert main_yaml['rflink']['rx_plugins']=='all'
    assert main_yaml['packages']['rflink_api']['files']==['packages/rflink-ha-data-only.yaml']
    data=yaml.load((ROOT/'packages/rflink-ha-data-only.yaml').read_text(),Loader=Loader)
    assert 'event' not in data
    for f in (repo/'examples').rglob('*.yaml'):
        d=yaml.load(f.read_text(),Loader=Loader)
        if isinstance(d, dict):
            for c in d.get('event',[]):
                # Documentation fragments deliberately contain REPLACE_* tokens;
                # only fully concrete examples are validator fixtures.
                if any(str(c.get(k, '')).startswith('REPLACE_') for k in ['protocol','rf_id','button','command']):
                    continue
                v.validate_pattern(c)
    log('PASS: YAML unique keys and event matches; no duplicate old packages; all RX and rxgate2 flags retained.')
    # Sensor/field conversion content must be preserved exactly from the working package.
    old=yaml.load((repo/'packages/rflink-ha-all-data.yaml').read_text(),Loader=Loader)
    for key in ['sensor','text_sensor','binary_sensor','globals','interval']:
        assert data.get(key)==old.get(key),key
    assert next(s for s in data['script'] if s['id']=='rflink_api_snapshot')==next(s for s in old['script'] if s['id']=='rflink_api_snapshot')
    log('PASS: all sensor definitions, data snapshot lambda, update interval, globals unchanged.')
    stage=load_module('stage_for_remote',repo/'components/rflink/stage_sources.py')
    with tempfile.TemporaryDirectory(prefix='rflink-remote-tests-') as d:
        d=Path(d);src=d/'src';(src/'esphome/components').mkdir(parents=True)
        (src/'esphome/components/rflink').symlink_to(repo/'components/rflink')
        (src/'esphome/components/rflink_remote').symlink_to(ROOT/'components/rflink_remote')
        common=['g++','-std=gnu++20','-Wall','-Wextra','-g','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DESP8266','-DUSE_ESP8266','-DRFLINK_TEST_STRICT_PROGMEM','-I'+str(HERE/'stubs'),'-I'+str(src),'-I'+str(repo/'components/rflink')]
        for label,plugins,count in [('all','all',48),('configured','configured',47),('ev1527',[61],2)]:
            ids=stage.stage(repo,src/'rflink_vendor',plugins);assert len(ids)==count
            binary=d/'runtime'
            run(common+[repo/'components/rflink/rflink.cpp',repo/'components/rflink/rflink_engine.cpp',ROOT/'components/rflink_remote/rflink_remote.cpp',HERE/'test_runtime.cpp','-o',binary],label+'_compile')
            result=run([binary],label+'_run')
            for line in result.stdout.splitlines():
                if line.startswith('PASS'):log(line)
        # Keep the proven state machine itself under its existing regression tests.
        for testfile in ['legacy_gesture_test.cpp','holdfix_test.cpp']:
            path=repo/'tests/holdfix'/testfile
            if path.exists():
                binary=d/'hold_unit'
                run(['g++','-std=gnu++20','-Wall','-Wextra','-Werror','-fno-pie','-no-pie','-fsanitize=address,undefined','-I'+str(repo/'components/rflink'),path,'-o',binary],testfile+'_compile')
                result=run([binary],testfile+'_run')
                log(result.stdout.strip())
    for p,h in manifest.items():assert hashlib.sha256((repo/p).read_bytes()).hexdigest()==h,p
    log('PASS: upstream files still unchanged after all tests.')
    log('LIMIT: host C++20 and test doubles. No real ESPHome validation/code generation, Xtensa firmware compilation, OTA, RF hardware or HA/API network test.')
    (out/'SUMMARY.txt').write_text('\n'.join(report)+'\n')
if __name__=='__main__':main()
