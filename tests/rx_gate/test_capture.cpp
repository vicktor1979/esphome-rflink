// Host regression test: real patched receiver + real RFLink 0.1.5 engine.
// GPIO, timing, ESPHome base classes and Wi-Fi/API flags are TEST DOUBLES.
#include "remote_receiver.h"
#include "rflink.h"
#include "rflink_engine.h"
#include <Arduino.h>
#include "esphome/core/log.h"
#include <cassert>
#include <iostream>
#include <new>
#include <cstdlib>
#include <string>

uint32_t capture_clock=0, test_millis=0;
unsigned array_allocations=0;
bool allocation_fail=false;
void *operator new[](std::size_t n,const std::nothrow_t&) noexcept {
 if(allocation_fail) return nullptr;
 ++array_allocations; return std::malloc(n);
}
// All capture buffers live in static firmware-lifetime objects, like on device.
esphome::InternalGPIOPin rf_pin;
esphome::remote_receiver::RemoteReceiverComponent rf_receiver(&rf_pin);
esphome::rflink::RFLinkComponent rf_bridge;
esphome::rflink_data::ReadyGate rf_start_gate;
struct FakeWifi {bool connected=false; bool is_connected() const {return connected;}} wifi_main;
struct FakeSwitch {bool state=true;} rf_decode_test;
struct FakeBinary {
 bool state=false,has=false;
 bool has_state()const{return has;}
 void publish_state(bool v){state=v;has=true;}
} rf_decode_active;
bool rf_ota_active=false,rf_probe_api_states=false;
uint32_t rf_probe_frames=0;
std::string last_json;
unsigned messages=0;
struct CountListener : esphome::remote_base::RemoteReceiverListener {
 bool on_receive(esphome::remote_base::RemoteReceiveData)override{++rf_probe_frames;return false;}
} counter;
void advance(uint32_t us){capture_clock+=us;test_millis=capture_clock/1000;}
void at(uint32_t ms){capture_clock=ms*1000;test_millis=ms;}
void emit_frame(uint32_t code=0x853728) {
 for(int bit=23;bit>=0;--bit){
  const bool value=((code>>bit)&1)!=0;
  rf_pin.set_level(true);advance(value?900:300);
  rf_pin.set_level(false);advance(value?300:900);
 }
 rf_pin.set_level(true);advance(300);
 rf_pin.set_level(false);advance(5500);
 rf_receiver.loop();
}
#include "yaml_gate_functions.inc"
void pass(const char *s){std::cout<<"PASS: "<<s<<'\n';}

int main(){
 rf_receiver.set_buffer_size(1000);rf_receiver.set_filter_us(100);rf_receiver.set_idle_us(5000);
 rf_receiver.set_capture_enabled(false);rf_bridge.setup();rf_bridge.set_decode_enabled(false);
 rf_receiver.register_listener(&rf_bridge);rf_receiver.register_listener(&counter);
 rf_bridge.add_on_message_callback([](std::string s){++messages;last_json=s;});
 rf_receiver.setup();
 assert(!rf_pin.attached() && !rf_receiver.is_capture_enabled());
 assert(!esphome::HighFrequencyLoopRequester::is_high_frequency());
 assert(array_allocations==1);
 emit_frame();emit_frame();assert(rf_probe_frames==0&&rf_receiver.get_edge_count()==0);
 pass("capture_enabled=false prevents initial ISR installation, packets and fast-loop request");
 for(int i=0;i<5;++i){at(1000+i*1000);auto_step();}
 assert(!rf_receiver.is_capture_enabled() && rf_bridge.get_decode_calls()==0);
 pass("actual YAML gate stays OFF without Wi-Fi/API state subscription");
 wifi_main.connected=true;at(6000);auto_step();assert(!rf_pin.attached());
 rf_probe_api_states=true;at(7000);auto_step();at(11999);auto_step();assert(!rf_pin.attached());
 at(12000);auto_step();assert(rf_pin.attached()&&rf_bridge.is_decode_enabled());
 assert(esphome::HighFrequencyLoopRequester::is_high_frequency());
 pass("actual YAML starts capture + decode only after state subscription and 5000 ms");
 const unsigned attaches=rf_pin.attachments;
 rf_receiver.set_capture_enabled(true);auto_step();assert(rf_pin.attachments==attaches);
 pass("repeated ON is idempotent; does not reinstall ISR");
 for(int i=0;i<4;++i)emit_frame();
 assert(messages>0&&last_json.find("\"ID\":\"085372\"")!=std::string::npos);
 assert(rf_receiver.get_edge_count()>0 && rf_bridge.get_observed_frames()>0);
 pass("synthetic pin edges pass through actual receiver -> full 48-plugin engine -> EV1527 JSON");
 const unsigned allocations=array_allocations;
 rf_decode_test.state=false;manual_off();
 assert(!rf_pin.attached()&&!rf_receiver.is_capture_enabled()&&!rf_bridge.is_decode_enabled());
 const uint32_t edges=rf_receiver.get_edge_count(),frames=rf_probe_frames;
 for(int i=0;i<4;++i) { emit_frame(); }
 rf_receiver.loop();
 assert(edges==rf_receiver.get_edge_count()&&frames==rf_probe_frames);
 assert(!esphome::HighFrequencyLoopRequester::is_high_frequency());
 pass("OFF detaches only RX IRQ, stops fast loop, and produces no frames even with pin activity");
 rf_decode_test.state=true;auto_step();advance(5000000);auto_step();assert(rf_pin.attached());
 const unsigned messages_before=messages;
 rf_receiver.loop();assert(messages==messages_before);
 pass("resume drops paused/backlogged signal; no stale JSON replay");
 for(int i=0;i<1000;++i){rf_receiver.set_capture_enabled(false);rf_receiver.set_capture_enabled(true);}
 assert(array_allocations==allocations && esphome::HighFrequencyLoopRequester::requests==1);
 pass("1000 pause/resume cycles reuse buffer and balance fast-loop request");
 ota_begin();assert(!rf_pin.attached()&&!rf_bridge.is_decode_enabled()&&rf_ota_active);
 advance(10000000);auto_step();assert(!rf_pin.attached());
 ota_error();auto_step();advance(4999000);auto_step();assert(!rf_pin.attached());
 advance(1000);auto_step();assert(rf_pin.attached());
 pass("actual OTA lambdas stop capture and require fresh settle interval after error");
 wifi_main.connected=false;wifi_lost();assert(!rf_pin.attached()&&!rf_probe_api_states);
 wifi_main.connected=true;rf_probe_api_states=true;auto_step();advance(5000000);auto_step();assert(rf_pin.attached());
 pass("actual Wi-Fi disconnect lambda stops IRQ and resets readiness; reconnect resumes");
 rf_probe_api_states=false;auto_step();
 assert(!rf_pin.attached()&&!rf_bridge.is_decode_enabled());
 rf_probe_api_states=true;auto_step();advance(5000000);auto_step();assert(rf_pin.attached());
 pass("loss of API state subscription stops capture even when Wi-Fi is still connected");
 // Stop in the middle of a partial frame, then re-enable: its data must not leak.
 rf_pin.set_level(true);advance(300);rf_pin.set_level(false);advance(900);
 rf_pin.set_level(true);advance(300);rf_pin.set_level(false);advance(900);
 rf_receiver.set_capture_enabled(false);rf_receiver.set_capture_enabled(true);
 const unsigned before_partial_reset=messages;advance(6000);rf_receiver.loop();
 assert(messages==before_partial_reset);
 pass("pause/resume discards an in-progress partial frame");
 rf_receiver.on_shutdown();assert(!rf_pin.attached()&&!esphome::HighFrequencyLoopRequester::is_high_frequency());
 pass("shutdown leaves capture stopped");
 static esphome::InternalGPIOPin other_pin;
 static esphome::remote_receiver::RemoteReceiverComponent other(&other_pin);
 other.setup();assert(other_pin.attached());other.set_capture_enabled(false);
 pass("default capture-enabled behavior retained for configurations not opting out");
 static esphome::InternalGPIOPin fail_pin;
 static esphome::remote_receiver::RemoteReceiverComponent failed(&fail_pin);
 allocation_fail=true;failed.setup();allocation_fail=false;
 assert(failed.is_failed()&&!fail_pin.attached());failed.set_capture_enabled(true);assert(!fail_pin.attached());
 pass("allocation failure never enables IRQ or capture");
 static esphome::InternalGPIOPin bad_pin;
 static esphome::remote_receiver::RemoteReceiverComponent bad(&bad_pin);
 bad.set_buffer_size(2);bad.setup();assert(bad.is_failed()&&!bad_pin.attached());
 pass("invalid buffer cannot enable receiver");
 std::cout<<"LIMIT: host simulation; not ESP8266 target compilation or Wi-Fi/RF hardware validation.\n";
}
