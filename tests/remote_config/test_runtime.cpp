#include "esphome/components/rflink_remote/rflink_remote.h"
#include "esphome/components/rflink/rflink_engine.h"
#include <Arduino.h>
#include <cassert>
#include <algorithm>
#include <iostream>
#include <set>
using namespace esphome::rflink_remote;
uint32_t test_millis=0,probe_clock=0;
static unsigned checks=0;
static uint32_t next_epoch=0;
#define CHECK(...) do{assert((__VA_ARGS__));++checks;}while(0)
static std::vector<int32_t> raw(uint32_t code){
 std::vector<int32_t> v;for(int n=23;n>=0;--n){const bool b=(code>>n)&1;v.push_back(b?900:300);v.push_back(b?-300:-900);}v.push_back(300);v.push_back(-5000);return v;
}
static constexpr uint32_t ALL=(1u<<18)-1;
struct Rig {
 uint32_t epoch=0;
 esphome::rflink::RFLinkComponent bridge;
 RFRemoteHub hub;
 RFRemoteEvent one,two,otherbutton,selected,fast,message;
 esphome::binary_sensor::BinarySensor pressed;
 esphome::text_sensor::TextSensor signal,gesture;
 std::vector<std::string> jsons;
 Rig(){
   epoch=(next_epoch+=1000000); test_millis=epoch;
   auto config=[](RFRemoteEvent&e,const char*id,const char*b){e.set_pattern("EV1527",id,b,"ON",true);e.set_event_mask(ALL);e.set_log_events(false);
    e.set_event_types({"press","release","single","double","triple","click_4","click_5","click_6","click_7","click_8","click_9","click_10","hold","hold_repeat","hold_release","cancel","multi_overflow"});};
   config(one,"085372","08");config(two,"01fac2","08");config(otherbutton,"085372","04");
   config(selected,"085372","08");selected.set_event_mask((1u<<2)|(1u<<3)); // single/double only
   selected.set_event_types({"single","double"});
   config(fast,"085372","08");fast.set_event_mask((1u<<2)|(1u<<13)); // single + hold_repeat: no multi-click wait
   fast.set_event_types({"single","hold_repeat"});
   one.set_pressed_sensor(&pressed);
   message.set_pattern("Chuango","5ac8d7","02","ON",false);message.set_log_events(false);message.set_event_mask(1u<<17);message.set_event_types({"received"});message.set_message_cooldown(250);
   hub.set_parent(&bridge);hub.set_timing_values(180,450,180,350,700,250,30000);
   hub.configure_learning(false,60000,4,3,false);hub.set_learning_signal_sensor(&signal);hub.set_learning_gesture_sensor(&gesture);
   for(auto*p:{&one,&two,&otherbutton,&selected,&fast,&message})hub.add_remote(p);
   bridge.setup();bridge.set_log_messages(false);bridge.set_decode_enabled(false);hub.setup();
   bridge.add_on_message_callback([this](std::string s){jsons.push_back(std::move(s));});
 }
 void frame(uint32_t t,uint32_t code=0x853728){t+=epoch;test_millis=t;auto v=raw(code);bridge.on_receive(esphome::remote_base::RemoteReceiveData(v));hub.tick(t);}
 // Feed the documented accepted-frame interface to isolate routing from
 // overlaps between unchanged legacy RF decoders. This is not raw-RF validation.
 void accepted_frame(uint32_t t,uint32_t code){t+=epoch;test_millis=t;hub.observe_frame(61,code,t);hub.tick(t);}
 void accepted_burst(uint32_t t,uint32_t span,uint32_t code){for(uint32_t dt=0;dt<=span;dt+=40)accepted_frame(t+dt,code);}
 void end(uint32_t t){t+=epoch;test_millis=t;hub.tick(t);}
 void burst(uint32_t t,uint32_t span=80,uint32_t code=0x853728){for(uint32_t dt=0;dt<=span;dt+=40)frame(t+dt,code);}
 void enable(){bridge.set_decode_enabled(true);}
 void learn(uint32_t t){t+=epoch;test_millis=t;hub.set_learning_enabled(true);}
 void msg(uint32_t t,const std::string&s){t+=epoch;test_millis=t;hub.observe_message(s,t);}
};
static int gestures_in(const std::vector<std::string>&v,const char*type){int n=0;const std::string key=std::string(" · ")+type;for(const auto&s:v)if(s.size()>=key.size()&&s.compare(s.size()-key.size(),key.size(),key)==0)++n;return n;}
int main(){
 {Rig r;r.burst(1000);r.end(2000);CHECK(r.one.events.empty());CHECK(r.jsons.empty());CHECK(r.signal.history.empty());}
 {Rig r;r.enable();r.burst(1000);r.end(1700);CHECK(r.one.count("single")==1);CHECK(r.selected.count("single")==1);CHECK(r.two.events.empty());CHECK(r.otherbutton.events.empty());CHECK(!r.pressed.state);CHECK(r.signal.history.empty());CHECK(r.gesture.history.empty());}
 {Rig r;r.enable();r.burst(1000);r.end(1259);CHECK(r.fast.count("single")==0);r.end(1260);CHECK(r.fast.count("single")==1);r.end(1700);CHECK(r.fast.count("single")==1);}
 {Rig r;r.enable();r.burst(1000);r.burst(1300);r.end(2000);CHECK(r.one.count("double")==1);CHECK(r.one.count("single")==0);CHECK(r.selected.count("double")==1);CHECK(r.jsons.size()>=1);}
 {Rig r;r.enable();r.burst(1000);r.burst(1300);r.burst(1600);r.end(2300);CHECK(r.one.count("triple")==1);CHECK(r.one.count("double")==0);CHECK(r.selected.events.empty());}
 {Rig r;r.enable();r.burst(1000,80,0x1fac28);r.end(1700);r.accepted_burst(2000,80,0x853724);r.end(2700);CHECK(r.two.count("single")==1);CHECK(r.otherbutton.count("single")==1);CHECK(r.one.events.empty());}
 {Rig r;r.enable();r.burst(1000,1000);r.end(2180);int n=r.one.count("hold_repeat");r.end(2330);CHECK(r.one.count("hold_repeat")==n);r.burst(2350,1000);r.burst(3790,1000);r.end(5240);CHECK(r.one.count("hold")==1);CHECK(r.one.count("hold_release")==1);CHECK(r.one.count("press")==1);CHECK(r.one.count("single")==0);CHECK(!r.pressed.state);CHECK(r.selected.events.empty());}
 {Rig r;r.enable();r.burst(1000,1000);r.end(2450);r.burst(2800,1000);r.end(4250);CHECK(r.one.count("hold")==2);CHECK(r.one.count("hold_release")==2);}
 {Rig r;r.enable();r.burst(1000);r.bridge.set_decode_enabled(false);r.end(2000);CHECK(r.one.count("cancel")==1);CHECK(r.one.count("single")==0);CHECK(!r.pressed.state);r.enable();r.end(3000);CHECK(r.one.count("single")==0);}
 {Rig r;r.enable();r.learn(1000);r.accepted_burst(1100,80,0x123454);r.end(1800);CHECK(r.one.events.empty());CHECK(r.signal.state.find("012345")!=std::string::npos);CHECK(r.signal.state.find(" · 04 · ON · gestures")!=std::string::npos);CHECK(gestures_in(r.gesture.history,"single")==1);r.accepted_burst(2000,80,0x123454);r.end(2700);CHECK(gestures_in(r.gesture.history,"single")==2);}
 {Rig r;r.enable();r.learn(1000);r.accepted_frame(1100,0x222221);r.end(1800);CHECK(r.signal.history.empty());CHECK(r.gesture.history.empty());}
 {Rig r;r.enable();r.learn(1000);r.accepted_burst(1100,80,0x123454);r.accepted_burst(1400,80,0x123454);r.end(2100);CHECK(gestures_in(r.gesture.history,"double")==1);CHECK(gestures_in(r.gesture.history,"single")==0);}
 {Rig r;r.enable();r.learn(1000);r.accepted_burst(1100,80,0x123454);r.accepted_burst(1400,80,0x123454);r.accepted_burst(1700,80,0x123454);r.end(2400);CHECK(gestures_in(r.gesture.history,"triple")==1);}
 {Rig r;r.enable();r.learn(1000);r.accepted_burst(1100,1000,0x123454);r.accepted_burst(2450,1000,0x123454);r.end(3900);CHECK(gestures_in(r.gesture.history,"hold")==1);CHECK(gestures_in(r.gesture.history,"hold_repeat")>0);CHECK(gestures_in(r.gesture.history,"hold_release")==1);}
 {Rig r;r.enable();r.learn(1000);r.accepted_burst(1100,80,0x123454);r.accepted_burst(1400,80,0x987654);r.end(2100);CHECK(gestures_in(r.gesture.history,"single")==2);CHECK(gestures_in(r.gesture.history,"double")==0);}
 {Rig r;r.enable();r.learn(1000);for(uint32_t k=1;k<=4;k++){r.accepted_frame(1100+k*10,(k<<4)|1);}r.accepted_frame(1150,0x555554);CHECK(r.hub.get_learning_dropped_keys()==1);r.accepted_burst(2000,80,0x555554);r.end(2700);CHECK(r.signal.state.find("055555")!=std::string::npos);}
 {Rig r;r.enable();r.learn(1000);r.burst(1100);r.end(61000);CHECK(!r.hub.is_learning_enabled());auto n=r.gesture.history.size();r.burst(62000);r.end(63000);CHECK(r.gesture.history.size()==n);CHECK(r.one.count("single")>=1);}
 {Rig r;r.enable();r.learn(1000);r.burst(1100);test_millis=r.epoch+1200;r.hub.set_learning_enabled(false);r.end(1800);CHECK(r.one.count("single")==1);CHECK(r.gesture.history.empty());}
 {Rig r;r.enable();r.learn(1000);r.burst(1100,1000);r.bridge.set_decode_enabled(false);r.end(3000);CHECK(r.one.count("cancel")==1);CHECK(gestures_in(r.gesture.history,"cancel")==1);r.enable();r.end(4000);CHECK(gestures_in(r.gesture.history,"hold_release")==0);}
 // With all plugins the ambiguous 300/900-us 085372/04 fixture is accepted
 // by Eurodomest BEFORE EV1527. Keep that behavior; never fabricate EV gestures.
 {Rig r;r.enable();r.learn(1000);r.burst(1100,80,0x853724);r.end(1800);
  if(rflink_legacy::plugin_count()>4){CHECK(r.otherbutton.events.empty());CHECK(r.signal.state.find("Eurodomest")!=std::string::npos);CHECK(r.signal.state.find("message")!=std::string::npos);CHECK(r.gesture.history.empty());}
  else {CHECK(r.otherbutton.count("single")==1);CHECK(gestures_in(r.gesture.history,"single")==1);}
 }
 const std::string good=R"({"NAME":"Chuango","ID":"5ac8d7","SWITCH":"02","CMD":"ON"})";
 {Rig r;r.enable();r.msg(1000,good);r.msg(1100,good);r.msg(1250,good);CHECK(r.message.count("received")==2);CHECK(r.signal.history.empty());r.msg(2000,R"({"NAME":"Other","ID":"5ac8d7","SWITCH":"02","CMD":"ON"})");r.msg(2100,R"({"NAME":"Chuango","ID":"5ac8d7","SWITCH":"01","CMD":"ON"})");r.msg(2200,R"({"NAME":"Chuango","ID":"5ac8d7","SWITCH":"02","CMD":"OFF"})");CHECK(r.message.count("received")==2);CHECK(r.one.events.empty());}
 {Rig r;r.enable();r.learn(1000);r.msg(1100,good);CHECK(r.signal.state.find(" · message")!=std::string::npos);CHECK(r.gesture.history.empty());r.end(2000);CHECK(r.gesture.history.empty());}
 {Rig r;r.enable();r.learn(1000);r.msg(1100,R"({"NAME":"Cresta","ID":"abcd","TEMP":"00ea","BAT":"LOW"})");CHECK(r.signal.state.find("Cresta")!=std::string::npos);CHECK(r.signal.state.find("Cresta · abcd · message")!=std::string::npos);CHECK(r.gesture.history.empty());}
 {Rig r;r.enable();r.learn(1000);r.msg(1100,R"({"NAME":"EV1527","ID":"085372","SWITCH":"08","CMD":"ON"})");CHECK(r.signal.history.empty());CHECK(r.one.events.empty());CHECK(r.gesture.history.empty());}
 {Rig r;r.enable();r.learn(1000);r.msg(1100,"broken");r.msg(1200,R"({"NAME":"Chuango","ID":123,"SWITCH":"02","CMD":"ON"})");r.msg(1300,R"({"NAME":"Chuango","ID":"5ac8d7","SWITCH":2,"CMD":"ON"})");CHECK(r.signal.history.empty());CHECK(r.message.events.empty());}
 {const Match m{"Proto\"\\","0012","A1","UP"};const auto s=m.json("message");CHECK(!s.empty());CHECK(s.find("Proto\\\"\\\\")!=std::string::npos);CHECK(Match::ev1527(0x1fac28).rf_id=="01fac2");CHECK(Match::ev1527(0x853728).button=="08");CHECK((!Match{std::string(33,'x'),"1","",""}.valid()));}
 {Rig r;r.enable();r.learn(0xFFFFFF00u);r.accepted_burst(0xFFFFFF20u,80,0x123454);r.end(0x00000200u);CHECK(gestures_in(r.gesture.history,"single")==1);}
 std::cout<<"PASS "<<checks<<" runtime assertions; actual RFLink motor with "<<rflink_legacy::plugin_count()<<" RX plugins; raw known keys + accepted-frame routing fixtures; stub ESPHome entities/JSON.\n";
}
