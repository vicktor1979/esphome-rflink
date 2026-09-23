/* CAME TOP-432 RX adapter, v0.1.7. Based on protocol/timing in
 * https://github.com/cpainchaud/RFLink32/blob/master/RFLink/Plugins/Plugin_076.cpp
 * Original author: Christophe Painchaud (2021).
 * Source notice: free for use in any open source project when this header is included;
 * usage of parts in a commercial application is prohibited. RX only, no rolling-code bypass or TX.
 * Adaptation: bounds-checked signed microsecond view; no 36-pulse legacy gate.
 */
namespace rf_ext {
bool decode_076(const Pulses &p) {
  if(p.size()!=26 || !between(p[0],281,431)) return false;
  uint32_t code=0;
  for(size_t i=1;i<25;i+=2) {
    const bool a_short=between(p[i],180,464), b_short=between(p[i+1],180,464);
    const bool a_long=between(p[i],570,850), b_long=between(p[i+1],570,850);
    if(!((a_short&&b_long)||(a_long&&b_short))) return false;
    code=(code<<1)|(a_short?1U:0U);
  }
  if(repeated(76,code,1500)) return true;
  display_Header(); display_Name(PSTR("CAME-TOP432")); display_IDn(code,4);
  display_SWITCH(1); display_CMD(CMD_Single,CMD_On); display_Footer(); return true;
}
}
