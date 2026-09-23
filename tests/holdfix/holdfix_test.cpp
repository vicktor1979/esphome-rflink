#include "rflink_gestures.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
using namespace esphome::rflink_gestures;
struct Record { std::string type; Event event; };
struct Rig {
  Button button;
  std::vector<Record> records;
  Timing t;
  explicit Rig(bool grace=true) {
    if(grace){t.hold_release_ms=450;t.repeat_fresh_ms=180;}
    assert(button.configure_ev1527("085372","08",t));
    button.set_callback([this](const Event &e){records.push_back({e.type,e});});
  }
  int n(const std::string &s) const {
    return std::count_if(records.begin(),records.end(),[&](const auto &e){return e.type==s;});
  }
  void frame(uint32_t at){assert(button.observe(61,0x853728,at));button.tick(at);}
  void burst(uint32_t start,uint32_t span=100,uint32_t step=50){
    for(uint32_t dt=0;dt<=span;dt+=step)frame(start+dt);
  }
  void hold(uint32_t start=1000){burst(start,1000);assert(button.holding());}
};
int main(){
 int tests=0;
 // A continuous observed sequence with synthetic reception holes. The old
 // release timeout splits this sequence; the longer HELD timeout must not.
 for(bool grace:{false,true}){
   Rig r(grace);r.hold();
   uint32_t last=2000;
   for(uint32_t gap:{220u,350u,440u}){
     for(uint32_t dt=20;dt<gap;dt+=20)r.button.tick(last+dt);
     r.burst(last+gap,1000);last+=gap+1000;
   }
   r.button.tick(last+500);
   assert(r.n("hold")== (grace?1:4));
   assert(r.n("hold_release")== (grace?1:4));
   assert(r.n("press")== (grace?1:4));
   assert(r.n("single")==0 && !r.button.pressed());
   if(grace){assert(r.records.back().event.bridged_gaps==3);assert(r.records.back().event.max_gap_ms==440);}
   std::cout << (grace?"NEW":"LEGACY") << ": same synthetic holes 220/350/440 ms -> holds="
             <<r.n("hold")<<", releases="<<r.n("hold_release")<<"\n";++tests;
 }
 // Every gap below the held limit keeps the same stroke, even if tick precedes it.
 for(uint32_t gap:{179u,180u,220u,300u,449u}){
   Rig r;r.hold();r.button.tick(2000+gap);assert(r.button.holding());
   r.frame(2000+gap);assert(r.n("hold")==1 && r.n("hold_release")==0);
   r.button.tick(2000+gap+450);assert(r.n("hold_release")==1);++tests;
 }
 for(uint32_t gap:{450u,451u,700u}){
   Rig r;r.hold();r.frame(2000+gap);
   assert(r.n("hold_release")==1 && r.n("press")==2 && !r.button.holding());
   assert(r.records.back().event.gap_ms==gap);++tests;
 }
 // No repeat without new matching frames since the preceding repeat.
 {Rig r;r.hold();r.frame(2050);r.button.tick(2250);const int repeats=r.n("hold_repeat");
  r.button.tick(2499);assert(r.n("hold_repeat")==repeats);
  r.button.tick(2500);assert(r.n("hold_release")==1);++tests;}
 // Freshness expires at 180 ms, although the hold remains alive until 450 ms.
 {Rig r;r.hold();r.frame(2050);int repeats=r.n("hold_repeat");
  for(uint32_t t=2230;t<=2490;t+=10)r.button.tick(t);
  assert(r.button.holding() && r.n("hold_repeat")==repeats);
  r.button.tick(2500);assert(!r.button.pressed() && r.n("hold_release")==1);++tests;}
 // Resume after a radio hole without a new hold/press, and no catch-up burst.
 {Rig r;r.hold();r.frame(2050);r.button.tick(2300);int n=r.n("hold_repeat");
  r.frame(2400);assert(r.n("hold")==1 && r.n("press")==1);
  assert(r.n("hold_repeat")<=n+1);r.button.tick(2400);assert(r.n("hold_repeat")<=n+1);++tests;}
 // Short clicks keep their original 180 ms + 350 ms timing and identities.
 for(int count=1;count<=10;++count){
  Rig r;for(int i=0;i<count;++i)r.burst(1000+i*320);
  const uint32_t last=1000+(count-1)*320+100;
  r.button.tick(last+529);r.button.tick(last+530);
  const std::string type=count==1?"single":count==2?"double":count==3?"triple":"click_"+std::to_string(count);
  assert(r.n(type)==1 && r.n("hold")==0 && r.n("press")==count);++tests;
 }
 {Rig r;r.frame(1000);r.button.tick(5000);assert(r.n("hold")==0&&r.n("single")==1);++tests;}
 {Rig r;r.hold();assert(!r.button.observe(61,0x953728,2350));
  assert(!r.button.observe(13,0x853728,2400));r.button.tick(2450);
  assert(r.n("hold_release")==1);++tests;}
 {Rig r;r.hold();r.button.cancel(2100);r.button.tick(5000);
  assert(r.n("cancel")==1&&r.n("hold_release")==0&&!r.button.pressed());++tests;}
 {Rig r;r.burst(1000);r.button.tick(1300);r.button.cancel(1400);r.button.tick(5000);
  assert(r.n("cancel")==1&&r.n("single")==0);++tests;}
 {Rig r;for(uint32_t t=1000;t<=31200;t+=50)r.frame(t);
  assert(r.n("cancel")==1&&!r.button.pressed());r.frame(31500);assert(!r.button.pressed());
  r.button.tick(31950);r.frame(32000);assert(r.n("press")==2);++tests;}
 {Rig r;uint32_t begin=0xFFFFFE00u;r.hold(begin);r.frame(begin+1300);
  r.button.tick(begin+1750);assert(r.n("hold")==1&&r.n("hold_release")==1);++tests;}
 {Rig r;r.hold();int n=r.n("hold_repeat");r.button.tick(100000);
  assert(r.n("hold_repeat")==n&&r.n("hold_release")==1);++tests;}
 {Rig r;r.hold();r.button.tick(2457);assert(r.records.back().event.silence_ms==457);
  assert(r.records.back().event.at_ms==2457);r.frame(2512);assert(r.records.back().event.gap_ms==512);++tests;}
 // Documented limit: a pre-hold gap still delimits short clicks, not silently merged.
 {Rig r;r.burst(1000,300);r.burst(1600,300);r.button.tick(3000);
  assert(r.n("hold")==0&&r.n("double")==1);++tests;}
 // Documented ambiguity: a physical release/repress within the long held grace
 // is indistinguishable from a reception hole and remains the same hold.
 {Rig r;r.hold();r.burst(2300,100);assert(r.n("hold")==1&&r.n("press")==1);++tests;}
 {Timing t;t.hold_release_ms=100;assert(!t.valid());t.hold_release_ms=450;t.repeat_fresh_ms=451;assert(!t.valid());
  t.repeat_fresh_ms=180;assert(t.valid());t.hold_release_ms=2001;assert(!t.valid());++tests;}
 std::cout<<"PASS: "<<tests<<" holdfix scenarios; no RF/Wi-Fi hardware emulation.\n";
}
