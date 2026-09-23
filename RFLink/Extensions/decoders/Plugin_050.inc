/* Fine Offset WH2 family, RX adapter v0.1.7.
 * Protocol source: merbanan/rtl_433 src/devices/fineoffset.c (WH2 section only).
 * Copyright (C) 2017 Tommy Vestermark; Enhanced (C) 2019 Christian W. Zuckschwerdt.
 * Source file also credits (C) 2019 Marco Di Leo for WH51, which is NOT included here.
 * GPL-2.0-or-later; see licenses/GPL-2.0.txt.
 * Separate implementation of PWM extraction; strict packet size, type and CRC.
 * The ambiguous 47-bit WH5/Rosenborg variant is intentionally not guessed.
 */
namespace rf_ext {
bool decode_050(const Pulses &p) {
  if(p.size()!=96 && p.size()!=98 && p.size()!=110) return false;
  const unsigned bits=unsigned(p.size()/2), pre=bits==55?7:bits==49?9:8;
  uint8_t b[6]{};
  for(unsigned n=0;n<bits;++n) {
    const bool one=between(p[2*n],340,680), zero=between(p[2*n],1320,1680);
    if(!one&&!zero) return false;
    if(!between(p[2*n+1],800,n+1==bits?0x7FFFFFFFU:1200)) return false;
    if(n<pre) {if(!one)return false;}
    else put_bit(b,n-pre,one);
  }
  if((b[0]>>4)!=4 || crc8(b,4,0x31,0)!=b[4]) return false;
  if(bits==55) {uint8_t sum=0;for(unsigned i=0;i<5;++i)sum=uint8_t(sum+b[i]);if(sum!=b[5])return false;}
  if(b[3]!=0xFF && b[3]>100) return false;
  const bool tfa=(bits==55 && b[3]==0xFF);
  const unsigned raw=(unsigned(b[1]&15)<<8)|b[2];
  const int t=tfa?int(raw&0x7FF)-400:((raw&0x800)?-int(raw&0x7FF):int(raw));
  if(t < -500 || t > 1400) return false;
  if(repeated(50,signature(b,(bits-pre)/8))) return true;
  display_Header(); display_Name(tfa?PSTR("TFA-303225"):bits==55?PSTR("Fineoffset-WH2A"):
                                bits==49?PSTR("Fineoffset-TelldusProove"):PSTR("Fineoffset-WH2"));
  display_IDn(((b[0]&15)<<4)|(b[1]>>4),2);display_TEMP(temperature(t));
  if(b[3]!=0xFF)display_HUM(b[3],HUM_HEX);
  if(tfa)display_BAT((raw&0x800)==0); // WH2 itself does NOT supply this battery indication
  display_Footer();return true;
}
}
