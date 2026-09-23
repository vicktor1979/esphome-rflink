// Project-source notice: RFLink Gateway License v1.0; donor file has no separate license header.
// See ../licenses/RFLink-Gateway-original.txt and ../SOURCES.json. No blanket MIT claim.
// Avantek RX base format. Protocol: cpainchaud/RFLink32 Plugin_077.c.
// v0.1.7: bounded non-mutating PCM sync + PWM payload parser; retains full
// 32-bit address in repeat key. No TX/malloc helpers. Optional CRC-bearing
// variant is deliberately rejected: upstream reads that byte but does not
// verify its integrity. Support here is the documented no-CRC base format.
namespace rf_ext {
bool avantek_preamble(const Pulses &p,size_t s,unsigned &pairs) {
  pairs=0;
  while(pairs<8 && s+2*pairs+1<p.size() && between(p[s+2*pairs],380,580) &&
        between(p[s+2*pairs+1],380,580))++pairs;
  return pairs>=5;
}
bool decode_077(const Pulses &p) {
  if(p.size()<98)return false;
  for(size_t start=0;start+88<p.size();start+=2) {
    unsigned pairs=0;if(!avantek_preamble(p,start,pairs))continue;
    size_t pos=start+2*pairs; uint32_t used=0, sync=0;unsigned got=0;bool good=true;
    while(pos<p.size() && got<32) {
      const uint32_t duration=p[pos], units=(duration+240)/480;
      if(units<1 || units>6 || (duration>units*480?duration-units*480:units*480-duration)>160) {good=false;break;}
      const unsigned take=std::min<unsigned>(units,32-got);
      for(unsigned i=0;i<take;++i)sync=(sync<<1)|unsigned((pos&1)==0);
      got+=take;
      if(take<units){used=take*480;break;}
      ++pos;
    }
    if(!good||got!=32||sync!=0xCACA5353UL||(pos&1)||pos+71>=p.size())continue;
    uint8_t b[5]{};
    for(unsigned bit=0;bit<36;++bit,pos+=2) {
      if(pos+1>=p.size()){good=false;break;}
      const uint32_t high=p[pos]>used?p[pos]-used:0, low=p[pos+1];used=0;
      const bool one=between(high,380,580)&&between(low,760,bit==35?0x7FFFFFFFU:1160);
      const bool zero=between(high,760,1160)&&between(low,380,bit==35?0x7FFFFFFFU:580);
      if(!one&&!zero){good=false;break;}
      put_bit(b,bit,one);
    }
    if(!good)continue;
    // Do not silently accept a CRC trailer we cannot validate.
    if(pos<p.size()) {unsigned next=0;if(!avantek_preamble(p,pos,next))continue;}
    if(repeated(77,signature(b,5),1250))return true;
    display_Header();display_Name(PSTR("Avantek"));
    display_IDn((uint32_t(b[0])<<24)|(uint32_t(b[1])<<16)|(uint32_t(b[2])<<8)|b[3],8);
    display_SWITCH(b[4]>>4);display_Footer();return true; // no invented CMD=ON
  }
  return false;
}
}
