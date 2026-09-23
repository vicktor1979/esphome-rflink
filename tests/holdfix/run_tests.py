#!/usr/bin/env python3
"""HOST tests only: GNU C++20, ASan/UBSan. NOT a firmware or physical-radio test.
Requires Linux, g++, Python and PyYAML, plus the complete RFLink v0.1.5 repo.
python tests/holdfix/run_tests.py --repo /path/to/esphome-rflink --out /tmp/holdfix-results
"""
from pathlib import Path
import argparse, hashlib, importlib.util, json, os, re, subprocess, tempfile
import yaml
from integration_parts import PREAMBLE, TESTBODY
HERE=Path(__file__).resolve().parent
class Loader(yaml.SafeLoader): pass
def mapping(loader,node,deep=False):
    d={}
    for k,v in node.value:
        key=loader.construct_object(k,deep=deep)
        if key in d: raise ValueError('Duplicate YAML key: '+str(key))
        d[key]=loader.construct_object(v,deep=deep)
    return d
Loader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG,mapping)
for tag in ['!lambda','!secret','!include']:
    Loader.add_constructor(tag,lambda l,n:l.construct_scalar(n))
def translate(body,variables):
    for k,v in variables.items():body=body.replace('${'+k+'}',str(v))
    if '${' in body:raise ValueError('Unresolved substitution')
    return re.sub(r'id\((\w+)\)',r'\1',body)
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--repo',type=Path,required=True)
    ap.add_argument('--out',type=Path,default=Path('/tmp/holdfix-tests'))
    args=ap.parse_args();repo=args.repo.resolve();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    report=[]
    def log(s):print(s,flush=True);report.append(s)
    def run(cmd,label):
        env=dict(os.environ,LC_ALL='C',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',ASAN_OPTIONS='detect_leaks=1')
        r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=120,env=env)
        (out/(label+'.log')).write_text('$ '+' '.join(map(str,cmd))+'\n'+r.stdout+r.stderr+f'\nEXIT {r.returncode}\n')
        if r.returncode:raise RuntimeError(label+'\n'+r.stdout+r.stderr)
        return r
    original=json.loads((repo/'UPSTREAM_SHA256.json').read_text())
    assert len(original)==54
    for rel,digest in original.items():assert hashlib.sha256((repo/rel).read_bytes()).hexdigest()==digest,rel
    log('PASS: 54 upstream plugin/config/old files match the original manifest.')
    doc=yaml.load((repo/'packages/rflink-ha-gestures.yaml').read_text(),Loader=Loader)
    assert doc['substitutions']['rflink_gesture_hold_release_ms']=='450'
    assert doc['substitutions']['rflink_gesture_release_ms']=='180'
    assert doc['substitutions']['rflink_gesture_repeat_fresh_ms']=='180'
    assert doc['interval'][0]['interval']=='20ms'
    log('PASS: gesture YAML unique keys; separate 180/450/180 ms timing settings.')
    with tempfile.TemporaryDirectory(prefix='holdfix-') as d:
        d=Path(d);src=d/'src';src.mkdir();component=repo/'components/rflink'
        common=['g++','-std=gnu++20','-Wall','-Wextra','-g','-fno-pie','-no-pie',
                '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(component)]
        for name,file in [('legacy_unit','legacy_gesture_test.cpp'),('holdfix_unit','holdfix_test.cpp')]:
            run(common+['-Werror',HERE/file,'-o',d/name],name+'_compile')
            result=run([d/name],name+'_run');log(result.stdout.strip())
        spec=importlib.util.spec_from_file_location('stage_holdfix',component/'stage_sources.py')
        stage=importlib.util.module_from_spec(spec);spec.loader.exec_module(stage)
        setup=translate(doc['esphome']['on_boot']['then'][0]['lambda'],doc['substitutions'])
        tick=translate(doc['interval'][0]['then'][0]['lambda'],doc['substitutions'])
        types=','.join('"'+t+'"' for t in doc['event'][0]['event_types'])
        extra=r'''
 // Synthetic matching-frame gaps, NOT a replay of the user's raw RF signal.
 reset_case();rf_bridge.set_decode_enabled(true);
 burst(16000,1000);uint32_t last=17000;
 for(uint32_t gap:{220u,350u,440u}){
   end(last+180);const int n=rf_gesture_event_1.count("hold_repeat");
   end(last+gap-1);assert(rf_gesture_event_1.count("hold_repeat")==n);
   burst(last+gap,1000);last+=gap+1000;
 }
 end(last+450);
 assert(rf_gesture_event_1.count("press")==1);
 assert(rf_gesture_event_1.count("hold")==1);
 assert(rf_gesture_event_1.count("hold_release")==1);
 assert(rf_gesture_event_1.count("single")==0);
 assert(!rf_gesture_pressed_1.state);
 std::cout<<"PASS: YAML + original decoder chain, synthetic 220/350/440 ms holes remain ONE hold.\n";
'''
        body=TESTBODY.replace(' std::cout<<"PASS: actual YAML', extra+'\n std::cout<<"PASS: actual YAML')
        # The fixture has a prefixed sentence; insert before its final std::cout instead.
        if body==TESTBODY:
            pos=TESTBODY.rfind(' std::cout<<');assert pos>=0
            body=TESTBODY[:pos]+extra+TESTBODY[pos:]
        source=d/'integration.cpp'
        source.write_text(PREAMBLE+'\nvoid setup_gestures(){\nrf_gesture_event_1.allowed={'+types+'};\nrf_gesture_event_2.allowed=rf_gesture_event_1.allowed;\n'+setup+'\n}\nvoid tick_gestures(){\n'+tick+'\n}\n'+body)
        for name,selection in [('all','all'),('ev1527',[61])]:
            ids=stage.stage(repo,src/'rflink_vendor',selection)
            assert len(ids)==(48 if name=='all' else 2)
            cmd=common+['-DESP8266','-DUSE_ESP8266','-DRFLINK_TEST_STRICT_PROGMEM',
                 '-I'+str(HERE/'stubs'),'-I'+str(src),component/'rflink.cpp',component/'rflink_engine.cpp',
                 source,'-o',d/'integration']
            run(cmd,'integration_'+name+'_compile')
            result=run([d/'integration'],'integration_'+name+'_run')
            log('PASS: actual bridge, actual gesture YAML lambdas and '+str(len(ids))+' original RX plugins; guarded flash, ASan/UBSan.')
            for line in result.stdout.splitlines():
                if 'PASS:' in line:log(line)
    for rel,digest in original.items():assert hashlib.sha256((repo/rel).read_bytes()).hexdigest()==digest,rel
    log('PASS: all 54 upstream files still unchanged after testing.')
    log('LIMIT: host compiler/test doubles; no ESPHome generation, Xtensa firmware build, or physical RF/Wi-Fi/API/OTA test.')
    (out/'SUMMARY.txt').write_text('\n'.join(report)+'\n')
if __name__=='__main__':main()
