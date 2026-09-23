#include "rflink_gestures.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
using namespace esphome::rflink_gestures;
struct Fixture {
  Button button;
  std::vector<std::string> events;
  Fixture() {
    assert(button.configure_ev1527("085372", "08", Timing{}));
    button.set_callback([this](const Event &e) {events.emplace_back(e.type);});
  }
  void frame(uint32_t t) { assert(button.observe(61, 0x853728, t)); }
  void burst(uint32_t t, uint32_t span = 80, uint32_t gap = 40) {
    for (uint32_t dt = 0; dt <= span; dt += gap) frame(t+dt);
  }
  int count(const std::string &s) const { return std::count(events.begin(),events.end(),s); }
};
int main() {
  int tests=0;
  {Fixture f; f.burst(1000); f.button.tick(1609); assert(f.count("single")==0);
   f.button.tick(1610); assert((f.events==std::vector<std::string>{"press","release","single"})); ++tests;}
  {Fixture f; f.frame(1000); f.button.tick(10000);
   assert(f.count("hold")==0 && f.count("single")==1 && !f.button.pressed()); ++tests;}
  {Fixture f; f.burst(1000,600); f.button.tick(5000);
   assert(f.count("hold")==0 && f.count("single")==1); ++tests;}
  for (int n=2;n<=10;++n) {
    Fixture f;
    for (int i=0;i<n;++i) f.burst(1000+uint32_t(i)*300);
    f.button.tick(1000+uint32_t(n-1)*300+610);
    const std::string type=n==2?"double":n==3?"triple":"click_"+std::to_string(n);
    assert(f.count(type)==1 && f.count("single")==0 && f.count("hold")==0);
    assert(f.count("press")==n && f.count("release")==n); ++tests;
  }
  {Fixture f; for(int i=0;i<12;++i)f.burst(1000+uint32_t(i)*300); f.button.tick(5000);
   assert(f.count("multi_overflow")==1 && f.count("click_10")==0); ++tests;}
  {Fixture f; for(uint32_t t=1000;t<=2500;t+=20){ if((t-1000)%40==0)f.frame(t); f.button.tick(t); }
   assert(f.count("hold")==1 && f.count("hold_repeat")==3);
   f.button.tick(3000); assert(f.count("hold_release")==1 && f.count("single")==0 && !f.button.pressed()); ++tests;}
  {Fixture f; f.burst(1000); f.frame(1700); f.button.tick(2400);
   assert(f.count("single")==2 && f.count("double")==0); ++tests;}
  {Fixture f; f.frame(1000); f.frame(1150); f.frame(1300);
   assert(f.count("press")==1); f.button.tick(2000); assert(f.count("single")==1); ++tests;}
  {Fixture f; f.frame(1000); assert(!f.button.observe(61,0x853724,1100));
   assert(!f.button.observe(34,0x853728,1150)); f.button.tick(1530);
   assert(f.count("single")==1 && f.button.matching_frames()==1); ++tests;}
  {Fixture f; f.burst(1000); f.button.cancel(1100); f.button.tick(5000);
   assert(f.count("cancel")==1 && f.count("release")==0 && f.count("single")==0); ++tests;}
  {Fixture f; f.burst(1000); f.button.tick(1300); f.button.cancel(1400); f.button.tick(5000);
   assert(f.count("cancel")==1 && f.count("single")==0); ++tests;}
  {Fixture f; f.burst(1000,1000); f.button.cancel(2050); f.button.tick(10000);
   assert(f.count("hold")==1 && f.count("hold_release")==0 && f.count("cancel")==1); ++tests;}
  {Fixture f; f.button.cancel(0); f.button.cancel(200); assert(f.events.empty()); ++tests;}
  {Fixture f; f.burst(1000); f.burst(1300,1000); f.button.tick(5000);
   assert(f.count("hold")==1 && f.count("single")==0 && f.count("double")==0); ++tests;}
  {Fixture f; for(uint32_t t=1000;t<=32000;t+=40)f.frame(t);
   assert(f.count("cancel")==1 && f.count("press")==1 && !f.button.pressed());
   f.button.tick(32300); f.burst(32400); f.button.tick(33100);
   assert(f.count("press")==2 && f.count("single")==1); ++tests;}
  {Fixture f; const uint32_t start=0xFFFFFF00u; f.burst(start); f.button.tick(start+610);
   assert(f.count("single")==1); ++tests;}
  {Fixture f; const uint32_t start=0xFFFFFF00u; f.burst(start,1000);f.button.tick(start+1700);
   assert(f.count("hold")==1 && f.count("hold_release")==1); ++tests;}
  {Fixture f; f.burst(0); f.button.tick(610); assert(f.count("single")==1); ++tests;}
  {Fixture f; f.burst(1000,1000); int previous=f.count("hold_repeat"); f.button.tick(100000);
   assert(f.count("hold_repeat")==previous && f.count("hold_release")==1); ++tests;}
  {Button b; assert(!b.configure_ev1527("100000","08",Timing{}));
   assert(!b.configure_ev1527("085372","10",Timing{}));
   assert(!b.configure_ev1527("0x85372","08",Timing{}));
   assert(!b.configure_ev1527("","08",Timing{}));
   Timing t;t.release_ms=800;assert(!b.configure_ev1527("085372","08",t));
   t=Timing{};t.repeat_ms=0;assert(!b.configure_ev1527("085372","08",t));
   assert(b.configure_ev1527("01FAC2","0A",Timing{}));assert(b.code()==0x1FAC2A); ++tests;}
  {Fixture a,b; assert(b.button.configure_ev1527("01fac2","08",Timing{}));
   for(uint32_t t=1000;t<=2000;t+=40){a.frame(t); b.button.observe(61,0x853728,t);}
   assert(a.count("hold")==1 && b.events.empty()); ++tests;}
  std::cout<<"PASS: "<<tests<<" gesture scenarios (single/multi/hold/cancel/identity/rollover/safety).\n";
}
