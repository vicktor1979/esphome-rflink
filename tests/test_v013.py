#!/usr/bin/env python3
"""Host regression tests of actual package lambdas and the helper header.
Needs PyYAML, g++, libjson-c-dev. JSON uses a json-c backed mock, NOT ArduinoJson.
This does NOT validate ESPHome code generation, firmware size, OTA, Wi-Fi or API.
"""
from pathlib import Path
import importlib.util
import json
import re
import subprocess
import tempfile
import yaml
ROOT=Path(__file__).resolve().parents[1]
class Loader(yaml.SafeLoader):pass
def unique(loader,node,deep=False):
    out={}
    for k,v in node.value:
        key=loader.construct_object(k,deep=deep)
        if key in out:raise ValueError('Duplicate YAML key: '+str(key))
        out[key]=loader.construct_object(v,deep=deep)
    return out
Loader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG,unique)
Loader.add_constructor('!lambda',lambda l,n:l.construct_scalar(n))
Loader.add_constructor('!secret',lambda l,n:'SECRET_'+l.construct_scalar(n))
Loader.add_constructor('!include',lambda l,n:l.construct_scalar(n) if isinstance(n,yaml.ScalarNode) else l.construct_mapping(n))
def load(path):return yaml.load(path.read_text(encoding='utf-8'),Loader=Loader)
def translate(body,variables,global_ids):
    for k,v in variables.items():body=body.replace('${'+k+'}',str(v))
    if '${' in body:raise ValueError('Unresolved substitution in test: '+body)
    body=re.sub(r'id\((\w+)\)\.',r'\1.',body)
    for ident in global_ids:body=body.replace('id('+ident+')',ident)
    return re.sub(r'id\((\w+)\)',r'(&\1)',body)

STUBS=r'''
#include <json-c/json.h>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <type_traits>
#include <vector>
#include "rflink_fields.h"
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
uint32_t test_ms=1000;
uint32_t millis(){return test_ms;}
uint32_t micros(){return test_ms*1000u;}
struct TextSensor{
 std::string state; bool present=false; size_t writes=0;
 bool has_state()const{return present;}
 void publish_state(const std::string& s){state=s;present=true;++writes;}
};
struct Sensor{
 float state=NAN; bool present=false; size_t writes=0;
 bool has_state()const{return present;}
 void publish_state(float v){state=v;present=true;++writes;}
};
struct BinarySensor{
 bool state=false,present=false; size_t writes=0;
 bool has_state()const{return present;}
 void publish_state(bool v){state=v;present=true;++writes;}
 void invalidate_state(){present=false;++writes;}
};
struct Event{
 std::set<std::string> types; std::vector<std::string> received;
 explicit Event(std::set<std::string> t):types(std::move(t)){}
 void trigger(const std::string& s){assert(types.count(s));received.push_back(s);}
};
struct JsonVariant{
 json_object *value=nullptr;
 bool isNull()const{return !value || json_object_get_type(value)==json_type_null;}
 const char* operator|(const char *fallback)const{
  return value && json_object_get_type(value)==json_type_string ? json_object_get_string(value):fallback;
 }
 template<typename T>bool is()const{
  if(!value)return false;
  const auto type=json_object_get_type(value);
  if constexpr(std::is_same_v<T,const char*>)return type==json_type_string;
  if constexpr(std::is_same_v<T,bool>)return type==json_type_boolean;
  if constexpr(std::is_same_v<T,uint32_t>)return type==json_type_int &&
     json_object_get_int64(value)>=0 && json_object_get_int64(value)<=4294967295LL;
  if constexpr(std::is_same_v<T,int>)return type==json_type_int;
  if constexpr(std::is_same_v<T,float>)return type==json_type_int||type==json_type_double;
  return false;
 }
 template<typename T>T as()const{
  if constexpr(std::is_same_v<T,const char*>)return json_object_get_string(value);
  else return static_cast<T>(json_object_get_double(value));
 }
};
struct JsonObject{
 json_object *root;
 JsonVariant operator[](const char*key)const{json_object*v=nullptr;json_object_object_get_ex(root,key,&v);return{v};}
};
namespace json{
 template<typename F>bool parse_json(const std::string& text,F callback){
  json_tokener_error error;json_object*root=json_tokener_parse_verbose(text.c_str(),&error);
  if(!root||error!=json_tokener_success||json_object_get_type(root)!=json_type_object){if(root)json_object_put(root);return false;}
  const bool valid=callback(JsonObject{root});json_object_put(root);return valid;
 }
}
void assert_near(float a,float b){assert(std::fabs(a-b)<0.001f);}
'''

TEST=r'''
int main(){
 namespace data=esphome::rflink_data;
 const std::string remote=R"({"PARAM":"20;01","NAME":"EV1527","ID":"085372","SWITCH":"08","CMD":"ON"})";
 route(remote);
 assert(rflink_api_remote_1.received.size()==1);
 assert(rflink_api_received_count.writes==0);
 assert(!rflink_api_last_protocol.has_state());  // Snapshot has not been flushed yet.
 assert(rflink_api_snapshot_pending);
 flush();
 assert(rflink_api_last_protocol.state=="EV1527");
 assert(rflink_api_last_switch.state=="08");
 assert(!rflink_api_battery_low.has_state());
 assert(std::isnan(rflink_api_temp.state));
 route(remote);route(remote);route(remote);
 assert(rflink_api_remote_1.received.size()==4); // Events not coalesced!
 flush();
 assert(rflink_api_last_protocol.writes==1); // Unchanged diagnostics suppressed.
 route(R"({"NAME":"EV1527","ID":"01fac2","SWITCH":"08","CMD":"ON"})");
 assert(rflink_api_remote_2.received.size()==1);
 route(R"({"NAME":"Unknown","ID":"0001","SWITCH":"08","CMD":"ON"})");
 assert(rflink_api_remote_1.received.size()==4);
 // Coalesced packet: all present fields below come from the same latest source.
 route(R"({"PARAM":"20;7A","NAME":"SYNTHETIC","ID":"0007","SET_LEVEL":15,
   "TEMP":"00ea","HUM":43,"BARO":"03f5","HSTATUS":"01","BFORECAST":"04",
   "UV":"000a","LUX":"03e8","BAT":"LOW","RAIN":"008d","RAINRATE":"0005",
   "WINSP":"007b","AWINSP":"0064","WINGS":"0080","WINDIR":4,
   "WINCHL":"8037","WINTMP":"00c8","CHIME":2,"SMOKEALERT":"ON","PIR":"OFF",
   "CO2":800,"SOUND":42,"KWATT":"03e8","WATT":"00fa","CURRENT":123,
   "DIST":456,"METER":1000,"VOLT":230,"RGBW":"a5fe"})");
 flush();
 assert(rflink_api_last_id.state=="0007");assert(rflink_api_param_text.state=="20;7A");
 assert_near(rflink_api_set_level.state,15);assert_near(rflink_api_temp.state,23.4);
 assert_near(rflink_api_hum.state,43);assert_near(rflink_api_baro.state,1013);
 assert_near(rflink_api_uv.state,10);assert_near(rflink_api_lux.state,1000);
 assert_near(rflink_api_rain.state,14.1);assert_near(rflink_api_rainrate.state,.5);
 assert_near(rflink_api_winsp.state,12.3);assert_near(rflink_api_awinsp.state,10);
 assert_near(rflink_api_wings.state,128); // NO invented /10 for WINGS.
 assert_near(rflink_api_windir.state,90);assert_near(rflink_api_winchl.state,-5.5);
 assert_near(rflink_api_wintmp.state,20);assert_near(rflink_api_chime.state,2);
 assert_near(rflink_api_co2.state,800);assert_near(rflink_api_sound.state,42);
 assert_near(rflink_api_kwatt.state,1000);assert_near(rflink_api_watt.state,250);
 assert_near(rflink_api_current.state,123);assert_near(rflink_api_dist.state,456);
 assert_near(rflink_api_meter.state,1000);assert_near(rflink_api_volt.state,230);
 assert(rflink_api_rgbw_text.state=="a5fe");assert(rflink_api_invalid_fields_text.state.empty());
 assert(rflink_api_battery_low.has_state()&&rflink_api_battery_low.state);
 assert(rflink_api_smoke.has_state()&&rflink_api_smoke.state);
 assert(rflink_api_pir.has_state()&&!rflink_api_pir.state);
 assert(rflink_api_hstatus_text.state=="Komfortos");assert(rflink_api_forecast_text.state=="Eső");
 assert(rflink_api_last_message.state.size()<=250);
 assert(rflink_api_last_message.state=="SYNTHETIC · 0007");
 // A different source cannot inherit temperature/battery/smoke from the previous one.
 route(remote);flush();
 assert(std::isnan(rflink_api_temp.state));assert(std::isnan(rflink_api_co2.state));
 assert(!rflink_api_battery_low.has_state());assert(!rflink_api_pir.has_state());assert(!rflink_api_smoke.has_state());
 assert(rflink_api_battery_text.state=="Nincs adat");
 route(R"({"NAME":"SYNTHETIC","ID":"zero","TEMP":"8000","HUM":0,"RAIN":"0000","WINDIR":0,"PIR":"OFF","BAT":"OK"})");flush();
 assert_near(rflink_api_temp.state,0);assert_near(rflink_api_hum.state,0);assert_near(rflink_api_rain.state,0);
 assert(rflink_api_battery_low.has_state()&&!rflink_api_battery_low.state);
 assert(rflink_api_pir.has_state()&&!rflink_api_pir.state);
 route(R"({"NAME":"SYNTHETIC","ID":"bad","TEMP":"10000","HUM":101,"RAIN":141,"WINDIR":16,"PIR":"?","BAT":50})");flush();
 assert(std::isnan(rflink_api_temp.state));assert(std::isnan(rflink_api_hum.state));assert(std::isnan(rflink_api_rain.state));
 assert(!rflink_api_battery_low.has_state());assert(!rflink_api_pir.has_state());
 assert(rflink_api_invalid_fields_text.state.find("TEMP")!=std::string::npos);
 assert(rflink_api_invalid_fields_text.state.find("BAT")!=std::string::npos);
 const auto count=rflink_api_packet_count;
 route("not json");route("[]");route("null");route(R"({"NAME":"x"})");
 assert(rflink_api_packet_count==count);
 // Number formats: no prefix/junk/boolean/overflow. Scalar-width is a minimum, not a cap.
 uint32_t value=0;
 assert(data::parse_unsigned("ffffffff",16,0xffffffffu,value)&&value==0xffffffffu);
 assert(!data::parse_unsigned("100000000",16,0xffffffffu,value));
 assert(data::parse_unsigned("4294967295",10,0xffffffffu,value));
 assert(!data::parse_unsigned("4294967296",10,0xffffffffu,value));
 assert(!data::parse_unsigned("0xEA",16,65535,value));assert(!data::parse_unsigned("00eaX",16,65535,value));
 assert(!data::parse_unsigned("",16,65535,value));assert(!data::parse_unsigned("-001",16,65535,value));
 assert(!data::parse_unsigned(" 001",16,65535,value));
 // Per-device generator is exact protocol+ID, independent fields, expiry not emulated.
 device_route(R"({"NAME":"Cresta","ID":"00ab","TEMP":"00ea","HUM":43,"BAT":"LOW"})");
 assert_near(unit_temp.state,23.4);assert_near(unit_hum.state,43);assert(unit_bat.state);
 const auto writes=unit_temp.writes;
 device_route(R"({"NAME":"Cresta","ID":"00ab","TEMP":"00ea"})");
 assert(unit_temp.writes==writes+1);assert(unit_hum.state==43&&unit_bat.state);
 device_route(R"({"NAME":"Cresta","ID":"00cd","TEMP":"00ff","HUM":90,"BAT":"OK"})");
 device_route(R"({"NAME":"Mebus","ID":"00ab","TEMP":"00ff","HUM":90,"BAT":"OK"})");
 assert(unit_temp.writes==writes+1);assert_near(unit_temp.state,23.4);
 device_route(R"({"NAME":"Cresta","ID":"00ab","TEMP":"badHEX","BAT":"?"})");
 assert(std::isnan(unit_temp.state));assert(!unit_bat.has_state());
 // Complete JSON split, including a multibyte UTF-8 character at the 240-byte boundary.
 std::string large(239,'a');large+="ő";large+=std::string(780,'z');
 std::string joined;
 for(size_t i=0;i<5;++i){const auto part=data::json_part(large,i);assert(part.size()<=240);joined+=part;}
 assert(joined==large);assert(data::json_part("short",1).empty());
 // Gate: initially off, wait continuously 5 s, no 60 s timeout, reconnect wait, rollover.
 data::ReadyGate g;
 assert(!g.update(0,false,5000));assert(!g.update(1000,true,5000));
 assert(!g.update(5999,true,5000));assert(g.update(6000,true,5000));
 assert(g.update(66000,true,5000));assert(g.update(999999,true,5000));
 assert(!g.update(1000000,false,5000));assert(!g.update(1000001,true,5000));
 assert(g.update(1005001,true,5000));g.reset();
 assert(!g.update(0xfffffff0u,true,5000));assert(!g.update(0x20u,true,5000));
 assert(g.update(0x2000u,true,5000));assert(g.update(0xfffffff0u,true,5000));
 std::cout<<"PASS: exact routing + identical events + coalesced snapshots + all 27 numeric fields + metadata + BAT/PIR/SMOKE + invalidation + strict formats + per-device isolation + expiry refresh + UTF-8 JSON parts + API-ready gate/rollover\n";
}
'''

def main():
    core=load(ROOT/'packages/rflink-ha-all-data.yaml')
    device=load(ROOT/'examples/rflink.yaml')
    for path in ROOT.rglob('*.yaml'):load(path)
    assert device['rflink']['rx_plugins']=='all'
    assert 'dump' not in device['remote_receiver']
    assert device['remote_receiver']['buffer_size']=='1200b'
    assert len(core['event'])==2
    assert 'rf_decode_test_window' not in (ROOT/'examples/rflink.yaml').read_text()
    ids=[]
    for group in ['sensor','binary_sensor','text_sensor','event','globals','script','switch']:
        ids += [item['id'] for config in [core,device] for item in config.get(group,[]) if 'id' in item]
    assert len(ids)==len(set(ids)),'Duplicate IDs'
    spec=importlib.util.spec_from_file_location('maker',ROOT/'tools/make_rf_device.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    fixed=module.generate('unit','Test','Cresta','00ab',['TEMP','HUM','BAT'],'60min')
    all_fields=module.generate('all_unit','Test','Cresta','00ab',list(module.FIELDS)+list(module.BINARY)+list(module.TEXT),'60min')
    assert len(module.FIELDS)==27
    source=STUBS
    for group,typ in [('sensor','Sensor'),('text_sensor','TextSensor'),('binary_sensor','BinarySensor')]:
        for item in core.get(group,[])+fixed.get(group,[])+all_fields.get(group,[]):source+=typ+' '+item['id']+';\n'
    for item in core['event']:
        source+='Event '+item['id']+'({'+','.join(json.dumps(x) for x in item['event_types'])+'});\n'
    global_ids=[g['id'] for g in core['globals']]
    for g in core['globals']:source+=g['type']+' '+g['id']+' = '+g['initial_value']+';\n'
    variables=core['substitutions'].copy()
    source+='void route(const std::string &message){\n'+translate(core['script'][0]['then'][0]['lambda'],variables,global_ids)+'\n}\n'
    source+='void snapshot(const std::string &message){\n'+translate(core['script'][1]['then'][0]['lambda'],variables,global_ids)+'\n}\n'
    source+='void device_route(const std::string &message){\n'+translate(fixed['script'][0]['then'][0]['lambda'],variables,global_ids)+'\n}\n'
    source+='void all_fields_device_route(const std::string &message){\n'+translate(all_fields['script'][0]['then'][0]['lambda'],variables,global_ids)+'\n}\n'
    source+='struct Script { void execute(const std::string &m){snapshot(m);} } rflink_api_snapshot;\n'
    source+='void flush(){\n'+translate(core['interval'][0]['then'][0]['lambda'],variables,global_ids)+'\n}\n'
    source+=TEST
    with tempfile.TemporaryDirectory(prefix='rflink_v013_') as temp:
        p=Path(temp);(p/'test.cpp').write_text(source,encoding='utf-8')
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-pedantic','-O1','-g',
                        '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                        '-I'+str(ROOT/'components/rflink'),str(p/'test.cpp'),'-ljson-c','-o',str(p/'test')],check=True)
        subprocess.run([str(p/'test')],check=True)
    print('PASS: YAML syntax, unique keys/IDs, unchanged GPIO/timing/plugin selection; full device generator')
    print('NOT TESTED: real ESPHome validation/code generation, ArduinoJson, ESP8266 firmware build/flash size, Wi-Fi/API/OTA hardware, elapsed timeout scheduling.')
if __name__=='__main__':main()
