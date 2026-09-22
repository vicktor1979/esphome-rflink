#!/usr/bin/env python3
"""Host-only checks of the actual YAML lambdas, not an ESP8266/API emulator.

Needs Python + PyYAML, g++, libjson-c-dev. The harness uses json-c behind a
minimal JsonObject mock, plus sensor/event mocks. Actual ArduinoJson, ESPHome
code generation, sensor filters, firmware size and HA transport are NOT tested.
"""
from pathlib import Path
import json
import re
import subprocess
import tempfile
import yaml

ROOT = Path(__file__).resolve().parents[1]
class UniqueLoader(yaml.SafeLoader):
    pass

def unique_mapping(loader, node, deep=False):
    result = {}
    for key_node, val_node in node.value:
        key = loader.construct_object(key_node, deep=deep)
        if key in result:
            raise ValueError(f'Duplicate YAML key: {key}')
        result[key] = loader.construct_object(val_node, deep=deep)
    return result
UniqueLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, unique_mapping)

def load(path):
    return yaml.load(path.read_text(), Loader=UniqueLoader)

def cxx_lambda(body, variables, global_ids=()):
    for key, value in variables.items():
        body = body.replace('${' + key + '}', str(value))
    assert '${' not in body, 'Unresolved substitution'
    # Match ESPHome lambda ID semantics: dot -> object access; bare entity ID ->
    # pointer; a globals component ID -> its stored value.
    body = re.sub(r'id\((\w+)\)\.', r'\1.', body)
    for name in global_ids:
        body = body.replace(f'id({name})', name)
    return re.sub(r'id\((\w+)\)', r'(&\1)', body)

STUBS = r'''
#include <json-c/json.h>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <set>
#include <string>
#include <type_traits>
#include <vector>
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
uint32_t test_ms = 1000;
uint32_t millis() { return test_ms; }
struct TextSensor {
  std::string state; bool present = false; size_t writes = 0;
  bool has_state() const { return present; }
  void publish_state(const std::string &s) { state=s; present=true; ++writes; }
};
struct Sensor {
  float state = NAN; size_t writes = 0;
  void publish_state(float value) { state=value; ++writes; }
};
struct BinarySensor {
  bool state = false, present = false; size_t writes = 0;
  void publish_state(bool value) { state=value; present=true; ++writes; }
};
struct Event {
  std::set<std::string> types;
  std::vector<std::string> received;
  explicit Event(std::set<std::string> t) : types(std::move(t)) {}
  void trigger(const std::string &s) { assert(types.count(s)); received.push_back(s); }
};
struct JsonVariant {
  json_object *value = nullptr;
  const char *operator|(const char *fallback) const {
    return value && json_object_get_type(value) == json_type_string ?
        json_object_get_string(value) : fallback;
  }
  template <typename T> bool is() const {
    if (!value) return false;
    const auto type = json_object_get_type(value);
    if constexpr (std::is_same_v<T, float>) return type == json_type_int || type == json_type_double;
    if constexpr (std::is_same_v<T, int>) return type == json_type_int;
    return false;
  }
  template <typename T> T as() const { return static_cast<T>(json_object_get_double(value)); }
};
struct JsonObject {
  json_object *root;
  JsonVariant operator[](const char *key) const {
    json_object *v=nullptr; json_object_object_get_ex(root, key, &v); return {v};
  }
};
namespace json {
  template<typename F> bool parse_json(const std::string &message, F callback) {
    json_tokener_error error;
    json_object *root=json_tokener_parse_verbose(message.c_str(), &error);
    if (!root || error != json_tokener_success || json_object_get_type(root) != json_type_object) {
      if (root) json_object_put(root);
      return false;
    }
    const bool result=callback(JsonObject{root}); json_object_put(root); return result;
  }
}
uint32_t rflink_api_packet_count = 0;
TextSensor rflink_api_last_protocol, rflink_api_last_id, rflink_api_last_switch;
TextSensor rflink_api_last_command, rflink_api_last_message;
Sensor test_weather_temperature, test_weather_humidity;
BinarySensor test_weather_battery_low;
'''

TEST_DEFAULT = r'''
int main() {
  const std::string known = R"({"PARAM":"20;01","NAME":"EV1527","ID":"085372","SWITCH":"08","CMD":"ON"})";
  route(known);
  assert(rflink_api_remote_1.received.size()==1);
  assert(rflink_api_remote_1.received.back()=="button_08");
  assert(rflink_api_remote_2.received.empty());
  assert(rflink_api_last_protocol.state=="EV1527");
  assert(rflink_api_last_id.state=="085372");
  assert(rflink_api_last_switch.state=="08");
  assert(rflink_api_last_command.state=="ON");
  assert(rflink_api_packet_count==1);
  route(known); // Identical JSON still emits a second event.
  assert(rflink_api_remote_1.received.size()==2);
  assert(rflink_api_packet_count==2);
  assert(rflink_api_last_protocol.writes==1); // Unchanged diagnostics suppressed.
  route(R"({"NAME":"EV1527","ID":"01fac2","SWITCH":"08","CMD":"ON"})");
  assert(rflink_api_remote_2.received.size()==1);
  route(R"({"NAME":"EV1527","ID":"085372","SWITCH":"0A","CMD":"ON"})");
  assert(rflink_api_remote_1.received.back()=="button_0a");
  const auto first_count=rflink_api_remote_1.received.size();
  const auto second_count=rflink_api_remote_2.received.size();
  route(R"({"NAME":"EV1527","ID":"123456","SWITCH":"08","CMD":"ON"})");
  route(R"({"NAME":"DifferentProtocol","ID":"085372","SWITCH":"08","CMD":"ON"})");
  route(R"({"NAME":"EV1527","ID":"085372","SWITCH":"08","CMD":"OFF"})");
  route(R"({"NAME":"EV1527","ID":"085372","SWITCH":"10","CMD":"ON"})");
  route(R"({"NAME":"EV1527","ID":"085372","SWITCH":"0g","CMD":"ON"})");
  route(R"({"NAME":"EV1527","ID":"085372","SWITCH":"8","CMD":"ON"})");
  assert(rflink_api_remote_1.received.size()==first_count);
  assert(rflink_api_remote_2.received.size()==second_count);
  route(R"({"NAME":"Cresta","ID":"1234","TEMP":"00ea","HUM":43})");
  assert(rflink_api_last_switch.state.empty());
  assert(rflink_api_last_command.state.empty());
  auto previous_count=rflink_api_packet_count;
  route("not json"); route("[]"); route("null");
  route(R"({"NAME":"EV1527","SWITCH":"08","CMD":"ON"})");
  route(R"({"NAME":null,"ID":"085372"})");
  assert(rflink_api_packet_count==previous_count);
  std::string long_frame = R"({"NAME":"Other","ID":"long","DATA":")" + std::string(400, 'x') + R"("})";
  route(long_frame);
  assert(rflink_api_last_message.state.size()<=250);
  assert(rflink_api_last_message.state.find("bajt")!=std::string::npos);
  assert(rflink_api_remote_1.received.size()==first_count);

  weather(R"({"NAME":"Cresta","ID":"1234","TEMP":"00ea","HUM":43,"BAT":"OK"})");
  assert(std::fabs(test_weather_temperature.state-23.4f)<0.001f);
  assert(test_weather_humidity.state==43);
  assert(test_weather_battery_low.present && !test_weather_battery_low.state);
  weather(R"({"NAME":"Cresta","ID":"1234","TEMP":"8037","HUM":0,"BAT":"LOW"})");
  assert(std::fabs(test_weather_temperature.state-(-5.5f))<0.001f);
  assert(test_weather_humidity.state==0 && test_weather_battery_low.state);
  weather(R"({"NAME":"Cresta","ID":"1234","TEMP":"0000"})");
  assert(test_weather_temperature.state==0);
  assert(test_weather_humidity.state==0 && test_weather_battery_low.state);
  auto temp_writes=test_weather_temperature.writes;
  auto hum_writes=test_weather_humidity.writes;
  weather(R"({"NAME":"Cresta","ID":"5678","TEMP":"00ff","HUM":60,"BAT":"OK"})");
  weather(R"({"NAME":"Mebus","ID":"1234","TEMP":"00ff","HUM":60,"BAT":"OK"})");
  assert(test_weather_temperature.writes==temp_writes);
  assert(test_weather_humidity.writes==hum_writes);
  weather(R"({"NAME":"Cresta","ID":"1234","TEMP":"zzzz","HUM":-1})");
  weather(R"({"NAME":"Cresta","ID":"1234","TEMP":"10000","HUM":101})");
  weather(R"({"NAME":"Cresta","ID":"1234","TEMP":"-001","HUM":"garbage"})");
  weather(R"({"NAME":"Cresta","ID":"1234","TEMP":234})");
  assert(test_weather_temperature.writes==temp_writes);
  assert(test_weather_humidity.writes==hum_writes);
  weather(R"({"NAME":"Cresta","ID":"1234","TEMP":"0000","HUM":0,"BAT":"LOW"})");
  assert(test_weather_temperature.writes==temp_writes+1); // Refresh expiry even if unchanged.
  assert(test_weather_humidity.writes==hum_writes+1);
  weather(R"({"NAME":"Cresta","ID":"1234","TEMP":"00EA"})");
  assert(std::fabs(test_weather_temperature.state-23.4f)<0.001f);
  std::cout << "PASS: default routing, identical events, IDs, all diagnostic fields, malformed/missing inputs, length guard, per-device temperature/humidity/battery\n";
}
'''
TEST_REPEAT = r'''
int main() {
  const std::string first = R"({"NAME":"EV1527","ID":"085372","SWITCH":"08","CMD":"ON"})";
  const std::string second = R"({"NAME":"EV1527","ID":"01fac2","SWITCH":"08","CMD":"ON"})";
  test_ms=1000; route(first);
  test_ms=1050; route(R"({"PARAM":"20;04","NAME":"EV1527","ID":"085372","SWITCH":"08","CMD":"ON"})");
  assert(rflink_api_remote_1.received.size()==1); // PARAM does not bypass optional rate limit.
  route(second); assert(rflink_api_remote_2.received.size()==1); // Independent remote.
  test_ms=1200; route(first); assert(rflink_api_remote_1.received.size()==2);
  test_ms=1210; route(R"({"NAME":"EV1527","ID":"085372","SWITCH":"01","CMD":"ON"})");
  assert(rflink_api_remote_1.received.size()==3); // Different button bypasses limit.
  test_ms=0xfffffff0u; route(first);
  test_ms=0x20u; route(first); assert(rflink_api_remote_1.received.size()==4);
  test_ms=0x100u; route(first); assert(rflink_api_remote_1.received.size()==5);
  std::cout << "PASS: optional 200 ms repeat rate limit, independent remotes/buttons, PARAM ignored, millis rollover\n";
}
'''

def main():
    core = load(ROOT/'packages/rflink-ha-api.yaml')
    optional = load(ROOT/'packages/rflink-ha-weather.yaml')
    expected={f'button_{n:02x}' for n in range(16)}
    for event in core['event']:
        assert set(event['event_types'])==expected
        assert event['internal'] is False
    assert 'mqtt' not in core and 'rflink' not in core and 'api' not in core
    assert 'remote_receiver' not in core  # No hidden change to working RF setup.
    assert len(core['text_sensor']) == 5
    for sensor in core['text_sensor']:
        assert sensor['internal'] is False and sensor['update_interval']=='never'
    print('PASS: package YAML syntax, no duplicate keys, 2 x 16 event types; no MQTT/API/radio/decoder override', flush=True)
    ev_types = '{' + ','.join(json.dumps(t) for t in sorted(expected)) + '}'
    declarations = f'Event rflink_api_remote_1({ev_types});\nEvent rflink_api_remote_2({ev_types});\n'
    weather_lambda=cxx_lambda(optional['script'][0]['then'][0]['lambda'], {
        'sensor_prefix':'test_weather', 'sensor_name':'Test',
        'rf_protocol':'Cresta', 'rf_device_id':'1234'})
    for repeat, test_code in ((0, TEST_DEFAULT), (200, TEST_REPEAT)):
        values = core['substitutions'] | {'rflink_api_repeat_ms':str(repeat)}
        main_lambda=cxx_lambda(core['script'][0]['then'][0]['lambda'], values, ('rflink_api_packet_count',))
        source = STUBS + declarations + '\nvoid route(std::string message) {\n' + main_lambda + '\n}\n'
        source += '\nvoid weather(std::string message) {\n' + weather_lambda + '\n}\n' + test_code
        with tempfile.TemporaryDirectory(prefix='rflink-native-') as directory:
            path=Path(directory)/'test.cpp'; path.write_text(source)
            binary=Path(directory)/'test'
            subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                            '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                            str(path),'-ljson-c','-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
    print('LIMIT: host mocks/json-c only. Not an ESPHome build, radio test, real ArduinoJson test, or Home Assistant API test.', flush=True)

if __name__=='__main__':
    main()
