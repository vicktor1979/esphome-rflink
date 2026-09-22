#include "rflink.h"
#include "rflink_engine.h"
#include <Arduino.h>
#include <cassert>
#include <string>
#include <iostream>
uint32_t test_millis=1000;
uint32_t probe_clock=0;
static std::vector<int32_t> ev1527(uint32_t bits) {
 std::vector<int32_t> frame;
 for (int i=23;i>=0;--i) { bool one=(bits>>i)&1; frame.push_back(one?900:300); frame.push_back(one?-300:-900); }
 frame.push_back(300);frame.push_back(-5000);return frame;
}
int main() {
 using esphome::remote_base::RemoteReceiveData;
 esphome::rflink::RFLinkComponent component;
 component.setup();
 assert(component.is_decode_enabled()); // old YAML remains enabled by default
 auto raw=ev1527(0x853728);
 std::string last;
 unsigned callbacks=0;
 component.add_on_message_callback([&](std::string s){last=std::move(s);++callbacks;probe_clock+=500;});
 component.set_decode_enabled(false);
 assert(!component.on_receive(RemoteReceiveData(raw)));
 assert(component.get_skipped_frames()==1 && component.get_decode_calls()==0);
 assert(component.get_message_count()==0 && callbacks==0);
 component.set_decode_enabled(true);
 assert(component.on_receive(RemoteReceiveData(raw)));
 assert(component.get_decode_calls()==1 && component.get_message_count()==1 && callbacks==1);
 assert(last.find("\"ID\":\"085372\"")!=std::string::npos);
 assert(last.find("\"SWITCH\":\"08\"")!=std::string::npos);
 assert(component.get_max_decode_us()==10);
 assert(component.get_max_callback_us()==510);
 // A repeated RF frame still uses the unchanged legacy deduplication logic.
 test_millis=1100;
 assert(component.on_receive(RemoteReceiveData(raw)));
 assert(component.get_decode_calls()==2 && component.get_message_count()==1 && callbacks==1);
 component.set_decode_enabled(false);
 assert(!component.on_receive(RemoteReceiveData(raw)));
 assert(callbacks==1 && component.get_skipped_frames()==2);
 // Resuming neither replays skipped events nor resets the legacy sequence.
 test_millis=3000;
 component.set_decode_enabled(true);
 assert(component.on_receive(RemoteReceiveData(raw)));
 assert(callbacks==2 && component.get_message_count()==2);
 assert(last.find("\"PARAM\":\"20;01\"")!=std::string::npos);
 component.set_decode_enabled(false);
 component.dump_config();
 std::cout << "PASS runtime gate: OFF skips decode+callback; ON decodes EV1527 085372/08; duplicate suppression; OFF/ON resume; diagnostic counters/timer arithmetic.\n";
}
