// Project-source notice: RFLink Gateway License v1.0; donor file has no separate license header.
// See ../licenses/RFLink-Gateway-original.txt and ../SOURCES.json. No blanket MIT claim.
// Silvercrest socket remote RX adaptation; protocol tables and timings from
// cpainchaud/RFLink32 RFLink/Plugins/Plugin_016.c (source accessed 2026-09-23).
// No transmitter code, mutable donor RawSignal or heap-allocated decoder helpers.
namespace rf_ext {
bool silvercrest_command(uint32_t code, bool &on) {
  static const uint16_t d_on[]={0xcd63,0xf7b4,0xb4f7,0x4ceb};
  static const uint16_t d_off[]={0xae81,0x8159,0x782a,0x9292};
  static const uint16_t five_on[]={0xfa1b,0x3da3,0x0254,0x8c27};
  static const uint16_t five_off[]={0x6e41,0x1bb9,0xa3fa,0xc002};
  static const uint16_t other_on[]={0xf756,0x7441,0xd9c5,0xe3aa,0x6af3,0x453f,0x0f6e,0xc170};
  static const uint16_t other_off[]={0x20e7,0x5212,0x9d88,0x8c0b,0x16bc,0x3b99,0xb8dd,0xae24};
  const unsigned remote=code>>20, command=(code>>4)&0xFFFF;
  const uint16_t *yes=remote==13?d_on:remote==5?five_on:other_on;
  const uint16_t *no=remote==13?d_off:remote==5?five_off:other_off;
  const unsigned n=(remote==13||remote==5)?4:8;
  bool found=false;
  for(unsigned i=0;i<n;++i) {
    if(command==yes[i]) {on=true;found=true;break;}
    if(command==no[i]) {on=false;found=true;break;}
  }
  if(found && (code&2U)) on=!on;
  return found;
}
bool decode_016(const Pulses &p) {
  // The source decodes a train. Require two consistent mapped frames, not arbitrary PWM.
  uint32_t candidate=0; bool candidate_on=false; unsigned copies=0;
  for(size_t s=0;s+48<p.size();s+=2) {
    if(s && p[s-1]<=2000) continue;
    uint32_t value=0; bool valid=true;
    for(unsigned b=0;b<24;++b) {
      const size_t i=s+2*b;
      const bool one=between(p[i],900,1400)&&between(p[i+1],200,600);
      const bool zero=between(p[i],200,600)&&between(p[i+1],900,1400);
      if(!one&&!zero) {valid=false;break;}
      value=(value<<1)|(one?1U:0U);
    }
    // One short closing mark followed by an actual inter-frame low gap.
    if(!valid || s+49>=p.size() || !between(p[s+48],200,600) || p[s+49]<=2000) continue;
    bool on=false; if(!silvercrest_command(value,on)) continue;
    const uint32_t logical=((value>>20)<<8)|(value&15);
    if(copies && candidate==logical && candidate_on==on) ++copies;
    else {candidate=logical;candidate_on=on;copies=1;}
    if(copies>=2) {
      if(repeated(16,(uint64_t(candidate)<<1)|unsigned(on))) return true;
      display_Header(); display_Name(PSTR("Silvercrest")); display_IDn(candidate>>8,1);
      display_SWITCH(candidate&15); display_CMD(CMD_Single,on?CMD_On:CMD_Off);
      display_Footer(); return true;
    }
    s+=48;
  }
  return false;
}
}
