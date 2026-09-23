// Byron SX: bounded repeated-train decoder using the supplied Plugin_072.c
// pulse examples and RFLink32's Multiply-aware timing. Original source/license
// header is preserved in overrides/Plugin_072.c. This new raw path needs TWO
// identical frames with correct boundaries; no arbitrary 12-bit command matching.
namespace rf_ext {
bool decode_072_raw(const Pulses &p) {
  uint32_t candidate=0;unsigned repeats=0;
  for(size_t s=0;s+25<p.size();s+=2) {
    if(s && !between(p[s-1],2400,3200)) continue;
    if(!between(p[s],150,425)) continue;
    uint32_t value=0;bool good=true;
    for(size_t i=s+1;i<s+25;i+=2) {
      const bool zero=between(p[i],150,349)&&between(p[i+1],450,650);
      const bool one=between(p[i],450,650)&&between(p[i+1],150,349);
      if(!zero&&!one){good=false;break;}
      value=(value<<1)|unsigned(one);
    }
    if(!good || !value || (!between(p[s+25],2400,3200) && s+26!=p.size())) continue;
    if(repeats && candidate==value)++repeats;else{candidate=value;repeats=1;}
    if(repeats<2)continue;
    if(repeated(72,value,1250)) return true;
    display_Header();display_Name(PSTR("Byron SX"));display_IDn(value>>4,4);
    display_SWITCH(1);display_CMD(CMD_Single,CMD_On);display_CHIME(value&15);
    display_Footer();return true;
  }
  return false;
}
}
