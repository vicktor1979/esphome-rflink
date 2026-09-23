// v0.1.7: bounded microsecond view. Included inside namespace rflink_legacy.
// The original byte-sized RawSignal ABI and RFLink/Plugins files are unchanged.
namespace rf_ext {
struct Pulses {
  const std::vector<int32_t> &v;
  size_t first{0};
  size_t count{0};
  bool valid{false};
  explicit Pulses(const std::vector<int32_t> &values) : v(values) {
    if (v.size() > 2048) return;  // deterministic processing budget, not a larger RX allocation
    while (first < v.size() && v[first] < 0) ++first;
    if (first == v.size()) return;
    for (size_t i=first; i<v.size(); ++i) {
      const int64_t x=v[i];
      if (x==0 || x==std::numeric_limits<int32_t>::min() || (((i-first)&1)==0)!=(x>0)) return;
    }
    count=v.size()-first;
    if (v.back()>0) ++count;  // virtual final timeout, never index outside vector
    valid=true;
  }
  size_t size() const { return count; }
  uint32_t operator[](size_t i) const {
    if (i >= count) return 0;
    if (i+first >= v.size()) return SIGNAL_END_TIMEOUT_US;
    const int64_t x=v[i+first]; return static_cast<uint32_t>(x<0?-x:x);
  }
};
inline bool between(uint32_t x, uint32_t low, uint32_t high) { return x>=low && x<=high; }
inline void put_bit(uint8_t *data, size_t i, bool bit) { if (bit) data[i/8] |= uint8_t(0x80U >> (i%8)); }
inline bool bit_at(const uint8_t *data, size_t i) { return (data[i/8] & uint8_t(0x80U >> (i%8)))!=0; }
inline uint16_t temperature(int deci_c) {
  return static_cast<uint16_t>(deci_c < 0 ? (0x8000U | static_cast<unsigned>(-deci_c)) : deci_c);
}
inline uint64_t signature(const uint8_t *data, size_t n) {
  uint64_t x=14695981039346656037ULL;
  for(size_t i=0;i<n;++i) { x^=data[i]; x*=1099511628211ULL; }
  return x;
}
struct History { uint64_t key{0}; uint32_t stamp{0}; uint8_t plugin{0}; bool used{false}; };
static History history[8];
static uint8_t history_cursor{0};
inline void reset_history() { for(auto &h:history) h=History{}; history_cursor=0; }
// Bounded, independent repeat state. Do not overwrite the legacy SignalCRC used by EV1527.
inline bool repeated(uint8_t plugin, uint64_t key, uint32_t timeout=1000) {
  const uint32_t now=millis(); History *slot=nullptr;
  for(auto &h:history) if(h.used && h.plugin==plugin && h.key==key) {
    if(static_cast<uint32_t>(now-h.stamp)<timeout) return true;
    slot=&h; break;
  }
  if(slot==nullptr) { slot=&history[history_cursor]; history_cursor=(history_cursor+1)%8; }
  *slot=History{key,now,plugin,true}; return false;
}
inline void channel(unsigned v) { decimal_field("CHAN",v); }
inline void wind_degrees(unsigned v) { decimal_field("WINDIR_DEG",v); }
inline void protocol_name(const char *prefix, unsigned model) {
  char name[32]; std::snprintf(name,sizeof(name),"%s-%04x",prefix,model); display_Name(name);
}
}  // namespace rf_ext
