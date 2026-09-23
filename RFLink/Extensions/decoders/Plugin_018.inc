// Project-source notice: RFLink Gateway License v1.0; donor file has no separate license header.
// See ../licenses/RFLink-Gateway-original.txt and ../SOURCES.json. No blanket MIT claim.
// Louvolite R1492-6CH-WH RX. Protocol: cpainchaud/RFLink32 Plugin_018.c.
// v0.1.7 adaptation: strict AC header, checksum and command validation;
// unique ID + SWITCH instead of duplicate ID JSON keys. RELEASE remains ignored.
namespace rf_ext {
bool decode_018(const Pulses &p) {
  if(p.size()!=130) return false;
  uint8_t b[9]{};
  for(size_t n=0;n<65;++n) {
    const size_t i=n*2;
    const bool one=between(p[i],615,715)&&between(p[i+1],216,n==64?0x7FFFFFFFU:316);
    const bool zero=between(p[i],216,316)&&between(p[i+1],615,n==64?0x7FFFFFFFU:715);
    if(!one&&!zero) return false;
    put_bit(b,n,one);
  }
  if(b[0]!=0xAC || (b[8]&0x80)==0) return false;
  uint8_t sum=0; for(unsigned i=1;i<=6;++i) sum=uint8_t(sum+b[i]);
  if(sum!=b[7]) return false;
  const uint16_t cmd=(uint16_t(b[5])<<8)|b[6];
  const char *command=cmd==0x000B?"UP":cmd==0x0023?"STOP":cmd==0x0043?"DOWN":nullptr;
  if(command==nullptr) return false;
  const uint32_t id=(uint32_t(b[1])<<16)|(uint32_t(b[2])<<8)|b[3];
  if(repeated(18,signature(b,8))) return true;
  display_Header(); display_Name(PSTR("Louvolite")); display_IDn(id,6);
  display_SWITCH(b[4]); display_CMDc(command); display_Footer(); return true;
}
}
