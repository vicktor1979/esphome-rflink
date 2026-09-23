/* LaCrosse TX141/TX145 RX adapter, v0.1.7.
 * Protocol source: merbanan/rtl_433 src/devices/lacrosse_tx141x.c
 * Copyright (C) 2017 Robert Fraczkiewicz; changes by Andrew Rivett.
 * GPL-2.0-or-later; see licenses/GPL-2.0.txt. RFLink32 Plugin_049.c supplied
 * the RFLink numbering and pulse windows. This is a bounded, reimplemented
 * microsecond pulse parser, not an embedded rtl_433 runtime or BitArray cast.
 */
namespace rf_ext {
bool decode_049(const Pulses &p) {
  uint8_t previous[9]{}; unsigned previous_bits=0, copies=0;
  for(size_t start=0; start+8+64<=p.size(); start+=2) {
    bool preamble=true;
    for(unsigned i=0;i<8;++i) if(!between(p[start+i],690,920)) {preamble=false;break;}
    if(!preamble) continue;
    uint8_t b[9]{}; unsigned bits=0; size_t pos=start+8;
    for(; bits<65 && pos+1<p.size(); ++bits,pos+=2) {
      const bool one=between(p[pos],370,620), zero=between(p[pos],150,320);
      if(!one&&!zero) break;
      const bool gap_ok=one?between(p[pos+1],150,320):between(p[pos+1],370,620);
      if(!gap_ok && p[pos+1]<1700) break;
      put_bit(b,bits,one);
      if(!gap_ok) {++bits;pos+=2;break;}
    }
    if(bits!=32 && bits!=33 && bits!=37 && bits!=40 && bits!=41 && bits!=64 && bits!=65) continue;
    if(bits>=64) {
      if((b[0]>>3)!=1 || crc8(b,8,0x31,0)!=0) continue;
      const unsigned type=b[3]&15, value=(unsigned(b[4])<<4)|(b[5]>>4);
      const unsigned other=(unsigned(b[5]&15)<<8)|b[6];
      if(type!=1 && type!=2) continue;
      if(type==1 && (other>100 || value<100 || value>1900)) continue;
      if(type==2 && other>359) continue;  // no 8-bit truncation or guessed compass-sector code
      if(repeated(49,signature(b,8))) return true;
      display_Header(); display_Name(PSTR("LaCrosse-TX141W"));
      display_IDn((uint32_t(b[0]&7)<<16)|(uint32_t(b[1])<<8)|b[2],5);
      channel((b[3]>>4)&3); display_BAT((b[3]&0x80)==0);
      if(type==1) {display_TEMP(temperature(int(value)-500));display_HUM(other,HUM_HEX);}
      else {display_AWINSP(value);wind_degrees(other);}
      display_Footer(); return true;
    }
    const bool th=(bits==40||bits==41);
    if(th && lfsr_digest8_reflect(b,4,0x31,0xF4)!=b[4]) continue;
    if(!th) {
      if(previous_bits==bits && std::memcmp(previous,b,(bits+7)/8)==0) ++copies;
      else {std::memcpy(previous,b,sizeof b);previous_bits=bits;copies=1;}
      if(copies<3) continue; // no checksum: require at least 3 matching copies in this capture
    }
    const unsigned value=(unsigned(b[1]&15)<<8)|b[2];
    if(!b[0] || value<100 || value>1900 || (th && (b[3]==0 || b[3]>100))) continue;
    if(repeated(49,signature(b,(bits+7)/8))) return true;
    display_Header(); display_Name(th?PSTR("LaCrosse-TX141THBv2"):bits==32?PSTR("LaCrosse-TX141B"):
                                   bits==33?PSTR("LaCrosse-TX141Bv3"):PSTR("LaCrosse-TX141Bv2"));
    display_IDn(b[0],2); channel((b[1]>>4)&3);
    display_TEMP(temperature(int(value)-500));
    display_BAT(th?((b[1]&0x80)==0):((b[1]&0x80)!=0));
    if(th) display_HUM(b[3],HUM_HEX);
    display_Footer();return true;
  }
  return false;
}
}
