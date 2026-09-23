// Compatibility implementation; original plugin and utility bytes are unmodified.
#include "rflink_engine.h"
#include <Arduino.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>

namespace rflink_legacy {
#include "rflink_vendor/2_Signal.h"
#include "rflink_vendor/4_Display.h"
#include "rflink_vendor/7_Utils.h"

RawSignalStruct RawSignal{};
unsigned long SignalCRC = 0, SignalCRC_1 = 0, RepeatingTimer = 0;
byte SignalHash = 0, SignalHashPrevious = 255;
boolean RFDebug = false, QRFDebug = false, RFUDebug = false, QRFUDebug = false;
char pbuffer[PRINT_BUFFER_SIZE]{};  // Legacy declaration; JSON never uses strcat.

namespace {
constexpr size_t MAX_JSON_SIZE = 1024;
std::string output;
byte sequence = 0;
bool started = false, finished = false, overflow = false;

void append(const char *text) {
  if (overflow || text == nullptr) return;
  const size_t len = std::strlen(text);
  if (output.size() + len > MAX_JSON_SIZE) { overflow = true; return; }
  output.append(text, len);
}
void escaped(const char *text) {
  append("\"");
  if (text != nullptr) {
    // Legacy plugins pass both PSTR/PROGMEM strings and RAM buffers as
    // const char*. ESP8266 cannot read flash with ordinary byte loads (*p).
    // pgm_read_byte() uses an aligned word access and also accepts RAM on
    // our supported ESP8266/ESP32 Arduino targets. Do not use strlen(),
    // std::string(text), or output.append(text) on these input pointers.
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); ; ++p) {
      const unsigned char value = pgm_read_byte(p);
      if (value == 0) break;
      if (value == '"') append("\\\"");
      else if (value == '\\') append("\\\\");
      else if (value < 0x20) {
        char encoded[7];
        std::snprintf(encoded, sizeof(encoded), "\\u%04x", static_cast<unsigned>(value));
        append(encoded);
      } else { char character[2] = {static_cast<char>(value), 0}; append(character); }
      if (overflow) break;
    }
  }
  append("\"");
}
void key(const char *name) {
  if (!started || finished) { overflow = true; return; }
  if (output.size() > 1) append(",");
  escaped(name); append(":");
}
void text_field(const char *name, const char *value) { key(name); escaped(value); }
void decimal_field(const char *name, unsigned long value) {
  char b[24]; std::snprintf(b, sizeof(b), "%lu", value); key(name); append(b);
}
void hex_field(const char *name, unsigned long value, unsigned width) {
  // Width is a minimum width, as in the original RFLink formatter.
  char b[24]; std::snprintf(b, sizeof(b), "%0*lx", static_cast<int>(std::min<unsigned>(width, 16U)), value);
  text_field(name, b);
}
void clear_message() { output.clear(); started = finished = overflow = false; }
}  // namespace

void display_Header() {
  clear_message(); started = true; append("{");
  char b[8]; std::snprintf(b, sizeof(b), "20;%02X", sequence++); text_field("PARAM", b);
}
void display_Name(const char *v) { text_field("NAME", v); }
void display_Footer() { if (started && !finished) { append("}"); finished = true; } }
void display_Splash() {}  // ESPHome dump_config replaces the original boot banner.
void display_IDn(unsigned long v, byte width) { hex_field("ID", v, width); }
void display_IDc(const char *v) { text_field("ID", v); }
void display_SWITCH(byte v) { hex_field("SWITCH", v, 2); }
void display_SWITCHc(const char *v) { text_field("SWITCH", v); }
void display_CMD(boolean group, byte cmd) {
  const char *value = cmd == CMD_On ? "ON" : cmd == CMD_Off ? "OFF" :
                      cmd == CMD_Bright ? "BRIGHT" : cmd == CMD_Dim ? "DIM" : "UNKNOWN";
  std::string command = group ? "ALL" : ""; command += value; text_field("CMD", command.c_str());
}
void display_CMDc(const char *v) { text_field("CMD", v); }
void display_SET_LEVEL(byte v) { decimal_field("SET_LEVEL", v); }
void display_TEMP(unsigned int v) { hex_field("TEMP", v, 4); }
void display_HUM(byte v, boolean type) {
  decimal_field("HUM", type == HUM_BCD ? ((v >> 4) * 10 + (v & 15)) : v);
}
void display_HSTATUS(byte v) { hex_field("HSTATUS", v, 2); }
void display_BFORECAST(byte v) { hex_field("BFORECAST", v, 2); }
void display_BAT(boolean v) { text_field("BAT", v ? "OK" : "LOW"); }
void display_SMOKEALERT(boolean v) { text_field("SMOKEALERT", v ? "ON" : "OFF"); }
void display_PIR(boolean v) { text_field("PIR", v ? "ON" : "OFF"); }
#define RFLINK_HEX_DISPLAY(NAME) void display_##NAME(unsigned int v) { hex_field(#NAME, v, 4); }
RFLINK_HEX_DISPLAY(BARO) RFLINK_HEX_DISPLAY(UV) RFLINK_HEX_DISPLAY(LUX)
RFLINK_HEX_DISPLAY(RAIN) RFLINK_HEX_DISPLAY(RAINRATE) RFLINK_HEX_DISPLAY(WINSP)
RFLINK_HEX_DISPLAY(AWINSP) RFLINK_HEX_DISPLAY(WINGS) RFLINK_HEX_DISPLAY(WINCHL)
RFLINK_HEX_DISPLAY(WINTMP) RFLINK_HEX_DISPLAY(KWATT) RFLINK_HEX_DISPLAY(WATT)
RFLINK_HEX_DISPLAY(RGBW)
#undef RFLINK_HEX_DISPLAY
#define RFLINK_DEC_DISPLAY(NAME) void display_##NAME(unsigned int v) { decimal_field(#NAME, v); }
RFLINK_DEC_DISPLAY(WINDIR) RFLINK_DEC_DISPLAY(CHIME) RFLINK_DEC_DISPLAY(CO2)
RFLINK_DEC_DISPLAY(SOUND) RFLINK_DEC_DISPLAY(CURRENT) RFLINK_DEC_DISPLAY(DIST)
RFLINK_DEC_DISPLAY(METER) RFLINK_DEC_DISPLAY(VOLT)
#undef RFLINK_DEC_DISPLAY

#include "rflink_vendor/7_Utils.cpp.inc"
#include "rflink_vendor/registry.inc"

void reset() {
  RawSignal = RawSignalStruct{};
#if RFLINK_PROFILE_EXTENDED
  rf_ext::reset_history();
#endif
  SignalCRC = SignalCRC_1 = RepeatingTimer = 0;
  SignalHash = 0; SignalHashPrevious = 255;
  sequence = 0; clear_message(); output.reserve(256);
}
size_t plugin_count() { return RFLINK_TOTAL_PLUGINS; }
const char *plugin_profile() { return RFLINK_PLUGIN_PROFILE; }

bool decode(const std::vector<int32_t> &timings, std::string &json, FrameObservation *observation) {
  if (observation != nullptr) *observation = FrameObservation{};
  json.clear(); clear_message(); RawSignal = RawSignalStruct{};
  if (timings.empty()) return false;
#if RFLINK_PROFILE_EXTENDED
  const rf_ext::Pulses pulses(timings);
  if (!pulses.valid) return false;
  for (const auto &extension : EXT_PLUGINS) {
    if (extension.decode == nullptr) break;
    if (extension.decode(pulses)) {
      if (finished && !overflow) json = output;
      return true;
    }
  }
  clear_message();
#endif
  size_t first = 0, end = timings.size();
  // ESPHome timings: positive mark, negative space. RFLink starts at a mark.
  while (first < end && timings[first] < 0) ++first;
  if (first == end) return false;
  // Normalize the last timeout space; RMT and GPIO backends can report it differently.
  if (end > first && timings[end - 1] <= -SIGNAL_END_TIMEOUT_US) --end;
  if (end == first) return false;
  const bool append_timeout = timings[end - 1] > 0;
  const size_t count = end - first + (append_timeout ? 1 : 0);
  if (count < MIN_RAW_PULSES || count > RAW_BUFFER_SIZE - 1) return false;
  RawSignal.Multiply = RAWSIGNAL_SAMPLE_RATE; RawSignal.Time = millis();
  size_t dest = 1;
  for (size_t pos = first; pos < end; ++pos) {
    const int64_t signed_value = timings[pos];
    const uint64_t us = signed_value < 0 ? -signed_value : signed_value;
    if (us == 0 || us > std::numeric_limits<int32_t>::max()) return false;
    // A malformed sequence is not a physical alternating waveform.
    if (((pos - first) % 2 == 0) != (signed_value > 0)) return false;
    const uint64_t ticks = us / RAWSIGNAL_SAMPLE_RATE;
    if (ticks == 0) return false;
    RawSignal.Pulses[dest++] = static_cast<byte>(std::min<uint64_t>(ticks, 255));
  }
  if (append_timeout) RawSignal.Pulses[dest++] = SIGNAL_END_TIMEOUT_US / RAWSIGNAL_SAMPLE_RATE;
  RawSignal.Number = static_cast<int>(dest - 1);
  // index 0 is a plugin marker, and Number+1 remains the original zero sentinel.
  for (size_t index = 0; index < sizeof(RX_PLUGINS)/sizeof(RX_PLUGINS[0]); ++index) {
    SignalHash = static_cast<byte>(index);
    if (RX_PLUGINS[index].decode(0, nullptr)) {
      // Plugin_061 validates the bits BEFORE its duplicate check. Both its new
      // frame path and its duplicate path return with SignalCRC == bitstream.
      // Do not reset CRC/timers, re-run the plugin, or bypass its legacy filter.
      // This mapping is deliberately limited to the uploaded EV1527 plugin.
      if (observation != nullptr && RX_PLUGINS[index].id == 61) {
        observation->valid = true;
        observation->plugin_id = 61;
        observation->code = static_cast<uint32_t>(SignalCRC) & 0x00FFFFFFUL;
      }
      SignalHashPrevious = SignalHash;
      RepeatingTimer = millis() + SIGNAL_REPEAT_TIME_MS;
      if (finished && !overflow) json = output;
      return true;  // includes duplicates deliberately suppressed by the plugin
    }
  }
  return false;
}
}  // namespace rflink_legacy
