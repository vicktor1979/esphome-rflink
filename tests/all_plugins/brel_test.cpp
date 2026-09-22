// Host-only integration fixture for the unchanged Plugin_083 receive code.
#include "rflink_engine.cpp"
#include <cassert>
#include <type_traits>
#include <iostream>
uint32_t test_millis=1000;
static void check_framework_macro_restored(){
  auto p = PSTR("original-const-type");
  static_assert(std::is_same_v<decltype(p),const char*>, "PSTR leaked beyond Plugin_083 include");
  assert(pgm_read_byte(p)=='o');
}
static std::vector<int32_t> make_frame(uint8_t cmd){
  const uint64_t code=(uint64_t(0x123456)<<16)|(uint64_t(0x08)<<8)|cmd;
  std::vector<int32_t> raw{4512,-1472};
  for(int i=39;i>=0;--i){ bool one=(code>>i)&1; raw.push_back(one?608:192);raw.push_back(one?-192:-608); }
  return raw;
}
int main(){
  using namespace rflink_legacy;
  check_framework_macro_restored();
  for(auto cmd:{0x11,0x33,0x55,0xcc}){
    reset();test_millis+=1000;
    std::string json; FrameObservation observation;
    assert(decode(make_frame(cmd),json,&observation));
    assert(!observation.valid); // No EV1527 gesture for Brel.
    assert(json.find("\"ID\":\"123456\"")!=std::string::npos);
    assert(json.find("BrelMotor")!=std::string::npos);
    std::cout<<json<<'\n';
  }
  // The source emits NAME twice (second one CMD=...). This test intentionally
  // preserves/reports that known upstream behavior; it does not certify a
  // normalized Home Assistant Brel command mapping.
  std::cout<<"PASS: four Brel command paths read protected PSTR; original PSTR const type restored.\n";
}
