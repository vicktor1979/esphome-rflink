#!/usr/bin/env python3
"""Compile the real sensor implementation with host ESPHome/JSON test doubles."""
from pathlib import Path
import subprocess
import os
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    d = Path(directory)
    def write(path, text):
        p = d/path
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text)
    write('esphome/core/component.h', '#pragma once\nnamespace esphome { class Component { public: virtual void setup() {} virtual void dump_config() {} }; }\n')
    write('esphome/core/log.h', '#pragma once\n#define LOG_SENSOR(...) ((void)0)\n#define ESP_LOGCONFIG(...) ((void)0)\n')
    write('esphome/components/sensor/sensor.h', '#pragma once\n#include <cmath>\nnamespace esphome { namespace sensor { class Sensor { public: float state=NAN; int writes=0; void publish_state(float f) {state=f; ++writes;} }; }}\n')
    write('esphome/components/rflink/rflink.h', '''#pragma once
#include <functional>
#include <vector>
#include <string>
#include "rflink_fields.h"
namespace esphome { namespace rflink { class RFLinkComponent {
public:
std::vector<std::function<void(const std::string&)>> observers;
void add_on_message_observer(std::function<void(const std::string&)> &&f) {observers.push_back(std::move(f));}
void emit(const std::string&s) {for (auto &f:observers) f(s);}
}; }}
''')
    parser = (ROOT/'tests/remote_config/stubs/esphome/components/json/json_util.h').read_text()
    parser = parser.replace('template<class T> bool is()const{return std::is_same<T,const char*>::value && string_type && !is_null;}', r"""template<class T> bool is()const {
 if constexpr(std::is_same_v<T,const char*>) return string_type && !is_null;
 if constexpr(std::is_same_v<T,bool>) return !string_type && (text=="true" || text=="false");
 if constexpr(std::is_same_v<T,uint32_t>) return !string_type && !is_null && !text.empty() && text.find_first_not_of("0123456789")==std::string::npos && std::stoull(text)<=4294967295ULL;
 return false;
 }
 template<class T> T as()const {
 if constexpr(std::is_same_v<T,const char*>) return text.c_str();
 else return static_cast<T>(std::stoul(text));
 }""")
    parser = parser.replace('JsonToken operator[](const char*k)const{auto p=fields.find(k);return p==fields.end()?JsonToken{}:p->second;}', 'const JsonToken &operator[](const char*k)const{static const JsonToken missing; auto p=fields.find(k);return p==fields.end()?missing:p->second;}')
    parser = parser.replace('namespace esphome { namespace json {', 'namespace esphome { namespace json {\ninline int parse_calls=0;')
    parser = parser.replace(' size_t pos=0;', ' ++parse_calls; size_t pos=0;')
    write('esphome/components/json/json_util.h', '#include <cstdint>\n'+parser)
    write('test.cpp', r'''
#include "rflink_sensor.h"
#include "esphome/components/json/json_util.h"
#include <cassert>
#include <iostream>
using namespace esphome;
int main() {
 rflink::RFLinkComponent bridge;
 rflink_sensor::RFLinkSensor a,b,h;
 a.set_parent(&bridge); a.set_match("Mebus","e501"); a.set_field(rflink_data::TEMP); a.setup();
 b.set_parent(&bridge); b.set_match("Alecto V1","006c"); b.set_field(rflink_data::TEMP); b.setup();
 h.set_parent(&bridge); h.set_match("Alecto V1","006c"); h.set_field(rflink_data::HUM); h.setup();
 bridge.emit(R"({"NAME":"Mebus","ID":"E501","TEMP":"010a"})");
 assert(std::fabs(a.state-26.6f)<0.001f && b.writes==0 && h.writes==0);
 bridge.emit(R"({"NAME":"Alecto V1","ID":"006C","TEMP":"8037","HUM":42})");
 assert(std::fabs(b.state+5.5f)<0.001f && h.state==42);
 bridge.emit(R"({"NAME":"Mebus","ID":"e501","TEMP":"0000"})"); assert(a.state==0 && a.writes==2);
 bridge.emit(R"({"NAME":"Mebus","ID":"e501","TEMP":"0000"})"); assert(a.writes==3);
 for (const char *bad : {
  R"({"NAME":"Other","ID":"e501","TEMP":"0010"})",
  R"({"NAME":"Mebus","ID":"0e501","TEMP":"0010"})",
  R"({"NAME":"Mebus","ID":"e502","TEMP":"0010"})",
  R"({"NAME":"Mebus","ID":"e501","TEMP":100})",
  R"({"NAME":"Mebus","ID":"e501","TEMP":"xxxx"})",
  R"({"NAME":"Mebus","ID":"e501","TEMP":"10000"})",
  R"({"NAME":"Mebus","ID":"e501","TEMP":null})",
  R"({"NAME":"Mebus","ID":"e501"})",
  R"({"TEMP":)"}) bridge.emit(bad);
 assert(a.state==0 && a.writes==3);
 const int calls=json::parse_calls;
 for(int i=0;i<10000;++i) bridge.emit(R"({"NAME":"EV1527","ID":"01fac2","SWITCH":"08","CMD":"ON"})");
 assert(json::parse_calls==calls && a.writes==3 && b.writes==1 && h.writes==1);
 std::cout << "PASS: exact device isolation; ID case/leading zeros; signed TEMP; HUM; missing/invalid retention; repeat refresh; 10000 EV messages skip JSON parsing.\n";
}
''')
    binary = d/'test'
    subprocess.run(['g++','-std=c++17','-g','-fno-pie','-no-pie','-fsanitize=address,undefined',
                    '-I'+str(d),'-I'+str(ROOT/'components/rflink_sensor'),'-I'+str(ROOT/'components/rflink'),
                    str(ROOT/'components/rflink_sensor/rflink_sensor.cpp'),str(d/'test.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True, env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1"))
