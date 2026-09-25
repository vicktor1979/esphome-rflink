// Host regression test: actual patched receiver + actual RFLink engine.
// GPIO, clocks and network/API state are deterministic TEST DOUBLES.
#include "remote_receiver.h"
#include "rflink.h"
#include "rflink_engine.h"
#include <Arduino.h>
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/network/util.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
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
esphome::api::APIServer api_server;
esphome::binary_sensor::BinarySensor decode_active;
esphome::text_sensor::TextSensor health,build;
std::string last_json;
unsigned messages=0;
void advance(uint32_t us){capture_clock+=us;test_millis=capture_clock/1000;}
void at(uint32_t ms){capture_clock=ms*1000;test_millis=ms;}
void service_bridge(){rf_bridge.loop();}
void emit_frame(uint32_t code=0x853728) {
 for(int bit=23;bit>=0;--bit){
  const bool value=((code>>bit)&1)!=0;
  rf_pin.set_level(true);advance(value?900:300);
  rf_pin.set_level(false);advance(value?300:900);
 }
 rf_pin.set_level(true);advance(300);
 rf_pin.set_level(false);advance(5500);
 rf_receiver.loop();
 service_bridge();
}
void pass(const char *s){std::cout<<"PASS: "<<s<<'\n';}

int main(){
 esphome::api::global_api_server=&api_server;
 rf_receiver.set_buffer_size(1200);rf_receiver.set_filter_us(100);rf_receiver.set_idle_us(5000);
 rf_receiver.set_high_frequency(false);rf_receiver.set_capture_enabled(false);
 rf_bridge.set_receiver(&rf_receiver);rf_bridge.set_auto_start_enabled(true);rf_bridge.set_auto_start_settle_ms(5000);
 rf_bridge.set_diagnostics_interval_ms(10000);rf_bridge.set_require_network(true);rf_bridge.set_require_api(true);
 rf_bridge.set_decode_active_sensor(&decode_active);rf_bridge.set_health_text_sensor(&health);rf_bridge.set_build_text_sensor(&build);
 rf_bridge.setup();rf_receiver.register_listener(&rf_bridge);
 rf_bridge.add_on_message_callback([](std::string s){++messages;last_json=s;});
 rf_receiver.setup();
 assert(!rf_pin.attached() && !rf_receiver.is_capture_enabled() && !rf_bridge.is_decode_enabled());
 assert(!esphome::HighFrequencyLoopRequester::is_high_frequency());
 assert(array_allocations==1);
 pass("built-in auto_start keeps capture/decode OFF during boot");

 // No network/API: never starts.
 for(int i=0;i<5;++i){at(1000+i*1000);service_bridge();}
 assert(!rf_receiver.is_capture_enabled() && rf_bridge.get_decode_calls()==0);
 pass("built-in gate stays OFF without network/API state subscription");

 esphome::network::test_network_connected=true;at(6000);service_bridge();assert(!rf_pin.attached());
 api_server.state_subscription_connected=true;at(7000);service_bridge();at(11999);service_bridge();assert(!rf_pin.attached());
 at(12100);service_bridge();assert(rf_pin.attached()&&rf_bridge.is_decode_enabled()&&decode_active.state);
 assert(!esphome::HighFrequencyLoopRequester::is_high_frequency());
 assert(health.state=="OK");
 pass("component starts IRQ capture + decode after 5 s stable network/API, without fast-loop request");

 const unsigned attaches=rf_pin.attachments;
 rf_receiver.set_capture_enabled(true);service_bridge();assert(rf_pin.attachments==attaches);
 pass("repeated ON is idempotent; does not reinstall ISR");
 for(int i=0;i<4;++i)emit_frame();
 assert(messages>0&&last_json.find("\"ID\":\"085372\"")!=std::string::npos);
 assert(rf_receiver.get_edge_count()>0 && rf_bridge.get_observed_frames()>0);
 pass("synthetic edges pass through receiver -> engine -> EV1527 JSON");

 // A long quiet period must not make the first fresh packet disappear.
 const unsigned before_idle=messages;
 at(test_millis + 10U*60U*1000U);service_bridge();
 emit_frame(0x853738); // distinct key, one frame must be enough after long idle
 assert(messages==before_idle+1 && last_json.find("\"SWITCH\":\"08\"")!=std::string::npos);
 assert(rf_bridge.get_repeat_history_resets()>0);
 pass("first EV1527 packet after 10 minutes idle is accepted; stale repeat history self-clears");

 const unsigned allocations=array_allocations;
 rf_bridge.set_monitoring_enabled(false);
 assert(!rf_pin.attached()&&!rf_receiver.is_capture_enabled()&&!rf_bridge.is_decode_enabled());
 const uint32_t edges=rf_receiver.get_edge_count();
 for(int i=0;i<4;++i)emit_frame();
 assert(edges==rf_receiver.get_edge_count());
 pass("monitoring OFF immediately detaches RX IRQ and decode");
 rf_bridge.set_monitoring_enabled(true);advance(100000);service_bridge();advance(4999000);service_bridge();assert(!rf_pin.attached());
 advance(1000);service_bridge();assert(rf_pin.attached()&&rf_bridge.is_decode_enabled());
 pass("monitoring ON requires a fresh settle interval");

 for(int i=0;i<1000;++i){rf_receiver.set_capture_enabled(false);rf_receiver.set_capture_enabled(true);}
 assert(array_allocations==allocations && esphome::HighFrequencyLoopRequester::requests==0);
 pass("1000 pause/resume cycles reuse buffer with no fast-loop request");

 const unsigned unchanged_attaches=rf_pin.attachments;
 const unsigned unchanged_detaches=rf_pin.detachments;
 const uint32_t unchanged_edges=rf_receiver.get_edge_count();
 for(int i=0;i<1000;++i) {
   rf_receiver.set_high_frequency(true);
   assert(rf_receiver.is_high_frequency_requested() && esphome::HighFrequencyLoopRequester::requests==1);
   rf_receiver.set_high_frequency(false);
   assert(!rf_receiver.is_high_frequency_requested() && esphome::HighFrequencyLoopRequester::requests==0);
 }
 assert(rf_pin.attached()&&rf_pin.attachments==unchanged_attaches&&rf_pin.detachments==unchanged_detaches);
 assert(unchanged_edges==rf_receiver.get_edge_count()&&array_allocations==allocations);
 pass("1000 fast-loop toggles preserve IRQ attachment, counters and allocation");

 rf_bridge.set_ota_active(true);assert(!rf_pin.attached()&&!rf_bridge.is_decode_enabled()&&rf_bridge.is_ota_active());
 advance(10000000);service_bridge();assert(!rf_pin.attached());
 rf_bridge.set_ota_active(false);advance(100000);service_bridge();advance(4999000);service_bridge();assert(!rf_pin.attached());
 advance(1000);service_bridge();assert(rf_pin.attached());
 pass("OTA state stops capture immediately and requires fresh settle after error/recovery");

 esphome::network::test_network_connected=false;service_bridge();assert(!rf_pin.attached()&&!rf_bridge.is_decode_enabled());
 esphome::network::test_network_connected=true;advance(100000);service_bridge();advance(5000000);service_bridge();assert(rf_pin.attached());
 pass("network disconnect/reconnect is handled inside component without YAML automation");
 api_server.state_subscription_connected=false;service_bridge();
 assert(!rf_pin.attached()&&!rf_bridge.is_decode_enabled());
 api_server.state_subscription_connected=true;advance(100000);service_bridge();advance(5000000);service_bridge();assert(rf_pin.attached());
 pass("loss of HA state subscription stops capture; reconnect resumes after settle");

 // Stop in the middle of a partial frame, then re-enable: its data must not leak.
 rf_pin.set_level(true);advance(300);rf_pin.set_level(false);advance(900);
 rf_pin.set_level(true);advance(300);rf_pin.set_level(false);advance(900);
 rf_receiver.set_capture_enabled(false);rf_receiver.set_capture_enabled(true);
 const unsigned before_partial_reset=messages;advance(6000);rf_receiver.loop();
 assert(messages==before_partial_reset);
 pass("capture reset discards an in-progress partial frame");

 rf_receiver.on_shutdown();assert(!rf_pin.attached()&&!esphome::HighFrequencyLoopRequester::is_high_frequency());
 pass("shutdown leaves capture stopped");
 static esphome::InternalGPIOPin other_pin;
 static esphome::remote_receiver::RemoteReceiverComponent other(&other_pin);
 other.setup();assert(other_pin.attached()&&other.is_high_frequency_requested());other.set_capture_enabled(false);
 pass("default capture-enabled/high-frequency behavior retained for old YAML without RFLink auto_start");
 static esphome::InternalGPIOPin fail_pin;
 static esphome::remote_receiver::RemoteReceiverComponent failed(&fail_pin);
 allocation_fail=true;failed.setup();allocation_fail=false;
 assert(failed.is_failed()&&!fail_pin.attached());failed.set_capture_enabled(true);assert(!fail_pin.attached());
 pass("allocation failure never enables IRQ/capture");
 static esphome::InternalGPIOPin bad_pin;
 static esphome::remote_receiver::RemoteReceiverComponent bad(&bad_pin);
 bad.set_buffer_size(2);bad.setup();assert(bad.is_failed()&&!bad_pin.attached());
 pass("invalid buffer cannot enable receiver");
 std::cout<<"LIMIT: host simulation; not ESP8266 target compilation or Wi-Fi/RF hardware validation.\n";
}
