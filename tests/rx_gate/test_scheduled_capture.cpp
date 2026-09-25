// Host-only timing tests. ISR calls, clock and polling are SIMULATED.
// The receiver .cpp and the complete 48-plugin RFLink engine are real sources.
#include "remote_receiver.h"
#include "rflink.h"
#include "rflink_engine.h"
#include <Arduino.h>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
uint32_t capture_clock=0,test_millis=0;
static esphome::InternalGPIOPin pin;
static esphome::remote_receiver::RemoteReceiverComponent rx(&pin);
static esphome::rflink::RFLinkComponent bridge;
static unsigned observations=0;
static std::vector<std::string> messages;
static uint32_t period=16000,next_poll=16000;
static void wait_us(uint32_t us){
 const uint32_t target=capture_clock+us;
 while(next_poll<=target){
   capture_clock=next_poll;test_millis=capture_clock/1000;next_poll+=period;
   rx.loop();
 }
 capture_clock=target;test_millis=capture_clock/1000;
}
static void frame(uint32_t code){
 for(int bit=23;bit>=0;--bit){
   const bool one=((code>>bit)&1)!=0;
   pin.set_level(true);wait_us(one?900:300);
   pin.set_level(false);wait_us(one?300:900);
 }
 pin.set_level(true);wait_us(300);pin.set_level(false);wait_us(12000);
}
static void pass(const char *s){std::cout<<"PASS: "<<s<<'\n';}
int main(){
 rx.set_buffer_size(1000);rx.set_filter_us(100);rx.set_idle_us(5000);
 rx.set_high_frequency(false);rx.set_capture_enabled(false);rx.setup();
 bridge.setup();bridge.set_decode_enabled(true);rx.register_listener(&bridge);
 bridge.add_on_message_callback([](std::string s){messages.push_back(s);});
 bridge.add_on_frame_callback([](uint16_t,uint32_t){++observations;});
 assert(rflink_legacy::plugin_count()==48);
 rx.set_capture_enabled(true);
 assert(pin.attached()&&!rx.is_high_frequency_requested());
 for(unsigned i=0;i<20;++i)frame(0x853728+(i<<4));
 wait_us(64000);
 std::cout<<"Scheduled capture: observations="<<observations<<" messages="<<messages.size()
          <<" polls="<<rx.get_loop_calls()<<" edges="<<rx.get_edge_count()
          <<" overflow_reports="<<rx.get_overflow_reports()<<'\n';
 assert(observations==20 && messages.size()==20);
 assert(rx.get_overflow_reports()==0);
 assert(messages.front().find("\"ID\":\"085372\"")!=std::string::npos);
 pass("20 distinct EV1527 frames decoded by all-plugin engine with loop() serviced every simulated 16 ms");
 // Toggle scheduling mid-frame: never reset/drop current capture when only mode changes.
 unsigned before=observations;
 const unsigned attaches=pin.attachments,detaches=pin.detachments;
 pin.set_level(true);wait_us(300);pin.set_level(false);wait_us(900); // MSB=0
 rx.set_high_frequency(true);rx.set_high_frequency(false);
 for(int bit=22;bit>=0;--bit){
   const bool one=((0x853728>>bit)&1)!=0;
   pin.set_level(true);wait_us(one?900:300);pin.set_level(false);wait_us(one?300:900);
 }
 pin.set_level(true);wait_us(300);pin.set_level(false);wait_us(32000);
 assert(pin.attachments==attaches&&pin.detachments==detaches&&observations==before+1);
 pass("changing high-frequency mode in mid-frame does not reset GPIO capture or lose synthetic frame");
 // Simulate a stalled consumer: overflow must be observable, not called packet loss count.
 const auto reports=rx.get_overflow_reports();
 const auto recoveries=rx.get_recovery_count();
 for(unsigned i=0;i<1600;++i){capture_clock+=300;test_millis=capture_clock/1000;pin.set_level(!pin.level);}
 rx.loop();next_poll=capture_clock+period;
 assert(rx.get_overflow_reports()>reports && rx.get_recovery_count()>recoveries);
 before=observations;frame(0x853728);wait_us(64000);
 assert(observations==before+1);
 pass("consumer-stall overflow is reported, ring indices self-reset, and next valid frame decodes without manual toggle");

 // Continuous sub-idle RF noise below ring capacity must also self-resync after
 // 2.5 s instead of leaving a partial capture alive indefinitely.
 const auto stall_recoveries=rx.get_recovery_count();
 for(unsigned i=0;i<850;++i){
   capture_clock+=3000;test_millis=capture_clock/1000;pin.set_level(!pin.level);
   if((i%4)==0) rx.loop();
 }
 rx.loop();next_poll=capture_clock+period;
 assert(rx.get_recovery_count()>stall_recoveries);
 // Let any few post-recovery noise edges close at the normal idle boundary.
 wait_us(6000);rx.loop();next_poll=capture_clock+period;
 before=observations;frame(0x853738);wait_us(64000);
 assert(observations==before+1);
 pass("continuous partial/noise capture auto-resynchronizes after 2.5 s and first later frame decodes");
 rx.on_shutdown();
 assert(!pin.attached()&&!esphome::HighFrequencyLoopRequester::is_high_frequency());
 std::cout<<"LIMIT: no ESPHome scheduler, Wi-Fi, Xtensa compiler or hardware in this simulation.\n";
}
