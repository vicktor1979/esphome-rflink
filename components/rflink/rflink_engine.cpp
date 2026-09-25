// Compatibility implementation; original plugin and utility bytes are unmodified.
// v0.1.9.5: Alecto V1 repeated-row soft recovery around untouched Plugin_030.
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
uint32_t plugin_enabled_mask[8]{};  // 256 plugin IDs, 32 bytes RAM.

// Alecto V1 sends the same 36-bit row several times in one RF burst. On the
// ESP8266 a weak/noisy 433 MHz signal can split a few long data gaps into
// smaller pieces while preserving the short separator pulses. The untouched
// Plugin_030 quite correctly rejects each damaged row by checksum. Keep a
// tiny, burst-local soft-vote buffer so several independently damaged repeats
// can reconstruct ONE checksum-valid row. Nothing is published unless the
// reconstructed row also passes the original Plugin_030 validation.
struct AlectoRepeatAccumulator {
  uint32_t last_ms{0};
  uint8_t frames{0};
  uint8_t zero_votes[36]{};
  uint8_t one_votes[36]{};
};
AlectoRepeatAccumulator alecto_repeat{};

void reset_alecto_repeat() { alecto_repeat = AlectoRepeatAccumulator{}; }

bool mask_get(uint16_t plugin_id) {
  if (plugin_id > 255) return false;
  return (plugin_enabled_mask[plugin_id >> 5] & (1UL << (plugin_id & 31))) != 0;
}
void mask_set(uint16_t plugin_id, bool enabled) {
  if (plugin_id > 255) return;
  const uint32_t bit = 1UL << (plugin_id & 31);
  if (enabled) plugin_enabled_mask[plugin_id >> 5] |= bit;
  else plugin_enabled_mask[plugin_id >> 5] &= ~bit;
}

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

constexpr size_t MAX_UNSUPPORTED_SUMMARY_SIZE = 240;

void build_unsupported_summary(int pulse_count, UnsupportedObservation *unsupported) {
  if (unsupported == nullptr || pulse_count <= 0) return;
  unsupported->valid = true;
  unsupported->pulse_count = static_cast<uint16_t>(std::min<int>(pulse_count, 0xFFFF));
  unsupported->truncated = false;
  unsupported->summary.clear();
  unsupported->summary.reserve(MAX_UNSUPPORTED_SUMMARY_SIZE);

  char header[48];
  std::snprintf(header, sizeof(header), "Pulses=%d; Pulses(uSec)=", pulse_count);
  unsupported->summary = header;

  for (int i = 1; i <= pulse_count; ++i) {
    char item[16];
    const unsigned value = static_cast<unsigned>(RawSignal.Pulses[i]) * RAWSIGNAL_SAMPLE_RATE;
    std::snprintf(item, sizeof(item), "%s%u", i == 1 ? "" : ",", value);
    const size_t item_len = std::strlen(item);
    // Reserve room for an explicit truncation marker. HA entity states are
    // deliberately kept comfortably below the traditional 255-byte limit.
    if (unsupported->summary.size() + item_len + 4 > MAX_UNSUPPORTED_SUMMARY_SIZE) {
      unsupported->truncated = true;
      unsupported->summary += ",...";
      break;
    }
    unsupported->summary += item;
  }
}

constexpr uint32_t ALECTO_REPEAT_WINDOW_MS = 2500;
constexpr uint32_t ALECTO_SEPARATOR_MAX_US = 900;
constexpr uint32_t ALECTO_ZERO_MIN_US = 1200;
constexpr uint32_t ALECTO_ZERO_MAX_US = 2500;
constexpr uint32_t ALECTO_ONE_MIN_US = 3000;
constexpr uint32_t ALECTO_ONE_MAX_US = 5500;
constexpr uint32_t ALECTO_GLITCH_MAX_US = 280;

bool alecto_bits_valid(const uint8_t bits[36]) {
  uint32_t bitstream = 0;
  for (uint8_t i = 0; i < 32; ++i) {
    bitstream >>= 1;
    if (bits[i] != 0) bitstream |= (0x1UL << 31);
  }
  uint8_t checksum = 0;
  for (uint8_t i = 32; i < 36; ++i) {
    checksum >>= 1;
    if (bits[i] != 0) checksum |= 0x08;
  }
  if (bitstream == 0) return false;

  uint8_t data[8]{};
  uint8_t checksumcalc = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    data[i] = static_cast<uint8_t>((bitstream >> (4 * i)) & 0x0F);
    checksumcalc = static_cast<uint8_t>(checksumcalc + data[i]);
  }
  if ((data[2] & 0x06) != 0x06)
    checksumcalc = static_cast<uint8_t>((0x0F - checksumcalc) & 0x0F);
  else if ((data[3] & 0x07) == 0x03)
    checksumcalc = static_cast<uint8_t>((0x07 + checksumcalc) & 0x0F);
  else
    checksumcalc = static_cast<uint8_t>((0x0F - checksumcalc) & 0x0F);
  if (checksum != checksumcalc) return false;

  // Mirror Plugin_030's range checks so a coincidental checksum can never turn
  // random 74-pulse traffic into a synthetic Alecto event.
  if ((data[2] & 0x06) != 0x06) {
    const uint8_t d3 = static_cast<uint8_t>(data[3] & 0x07);
    const int temperature = static_cast<int>((data[5] << 8) | (data[4] << 4) | d3);
    if ((temperature & 0x800) != 0) {
      if (4096 - temperature > 0x258) return false;
    } else if (temperature > 0x258) {
      return false;
    }
    const uint8_t humidity = static_cast<uint8_t>((data[7] << 4) | data[6]);
    if (humidity > 0x99) return false;
    return true;
  }

  // Plugin_030 only has meaningful payload handlers for these weather subtypes.
  const uint8_t subtype = static_cast<uint8_t>(data[3] & 0x07);
  return subtype == 0x03 || subtype == 0x01 || subtype == 0x07;
}

bool prepare_alecto_vote_pulses(const std::vector<int32_t> &timings, uint32_t pulses[75]) {
  if (timings.empty()) return false;
  size_t first = 0, end = timings.size();
  while (first < end && timings[first] < 0) ++first;
  if (first == end) return false;
  if (end > first && timings[end - 1] <= -SIGNAL_END_TIMEOUT_US) --end;
  if (end == first) return false;
  const bool append_timeout = timings[end - 1] > 0;
  size_t signal_count = end - first;
  size_t total_count = signal_count + (append_timeout ? 1U : 0U);
  if (total_count < 74 || total_count > 90 || ((total_count - 74) & 1U) != 0) return false;

  uint32_t work[90]{};
  if (signal_count > 89) return false;
  for (size_t i = 0; i < signal_count; ++i) {
    const int64_t signed_value = timings[first + i];
    const uint64_t us = signed_value < 0 ? -signed_value : signed_value;
    if (us == 0 || us > 20000) return false;
    work[i] = static_cast<uint32_t>(us);
  }

  // Frames with >74 pulses commonly contain narrow two-edge spikes. Collapse
  // only sub-280 us middle pulses; genuine Alecto V1 separators are ~325 us or
  // longer. Each collapse removes exactly two extra transitions.
  while (total_count > 74) {
    size_t best = signal_count;
    uint32_t best_width = ALECTO_GLITCH_MAX_US + 1;
    for (size_t i = 1; i + 1 < signal_count; ++i) {
      if (work[i] < best_width && work[i] < ALECTO_GLITCH_MAX_US) {
        best = i;
        best_width = work[i];
      }
    }
    if (best == signal_count) return false;
    work[best - 1] = work[best - 1] + work[best] + work[best + 1];
    for (size_t i = best; i + 2 < signal_count; ++i) work[i] = work[i + 2];
    signal_count -= 2;
    total_count -= 2;
  }
  if (signal_count != 73) return false;

  pulses[0] = 0;
  uint64_t total_us = 0;
  for (size_t i = 0; i < 73; ++i) {
    pulses[i + 1] = work[i];
    total_us += work[i];
  }
  pulses[74] = SIGNAL_END_TIMEOUT_US;
  // The user's damaged frame is ~114 ms, matching the original Plugin_030
  // examples. This broad guard rejects unrelated short/long 74-pulse packets.
  if (total_us < 80000 || total_us > 170000) return false;
  if (pulses[1] > ALECTO_SEPARATOR_MAX_US) return false;
  for (uint8_t x = 2; x <= 72; x += 2) {
    if (pulses[x + 1] > ALECTO_SEPARATOR_MAX_US) return false;
  }
  return true;
}

bool prepare_alecto_repeat_recovery(const std::vector<int32_t> &timings) {
  if (!mask_get(30)) return false;
  uint32_t pulses[75]{};
  if (!prepare_alecto_vote_pulses(timings, pulses)) return false;

  const uint32_t now = millis();
  if (alecto_repeat.last_ms == 0 || static_cast<uint32_t>(now - alecto_repeat.last_ms) > ALECTO_REPEAT_WINDOW_MS)
    reset_alecto_repeat();
  alecto_repeat.last_ms = now;

  uint8_t confident = 0;
  uint8_t frame_bits[36]{};
  uint8_t frame_known[36]{};
  for (uint8_t bit = 0; bit < 36; ++bit) {
    const uint32_t us = pulses[2 + bit * 2];
    if (us >= ALECTO_ZERO_MIN_US && us <= ALECTO_ZERO_MAX_US) {
      frame_known[bit] = 1;
      frame_bits[bit] = 0;
      ++confident;
    } else if (us >= ALECTO_ONE_MIN_US && us <= ALECTO_ONE_MAX_US) {
      frame_known[bit] = 1;
      frame_bits[bit] = 1;
      ++confident;
    }
  }
  // A real row should still contain mostly recognizable data gaps. This keeps
  // unrelated 74-pulse protocols out of the vote buffer.
  if (confident < 24) return false;

  if (alecto_repeat.frames < 255) ++alecto_repeat.frames;
  for (uint8_t bit = 0; bit < 36; ++bit) {
    if (!frame_known[bit]) continue;
    uint8_t &vote = frame_bits[bit] ? alecto_repeat.one_votes[bit] : alecto_repeat.zero_votes[bit];
    if (vote < 255) ++vote;
  }
  if (alecto_repeat.frames < 3) return false;

  uint8_t majority[36]{};
  for (uint8_t bit = 0; bit < 36; ++bit) {
    const uint8_t z = alecto_repeat.zero_votes[bit];
    const uint8_t o = alecto_repeat.one_votes[bit];
    if (static_cast<uint16_t>(z) + o < 2 || z == o) return false;
    majority[bit] = o > z ? 1 : 0;
  }
  if (!alecto_bits_valid(majority)) return false;

  // Recreate an ideal 74-pulse row. The caller immediately hands this to the
  // untouched Plugin_030, whose checksum/range/repeat logic remains final.
  RawSignal = RawSignalStruct{};
  RawSignal.Multiply = RAWSIGNAL_SAMPLE_RATE;
  RawSignal.Time = now;
  RawSignal.Number = 74;
  RawSignal.Pulses[1] = static_cast<byte>(480 / RAWSIGNAL_SAMPLE_RATE);
  for (uint8_t bit = 0; bit < 36; ++bit) {
    RawSignal.Pulses[2 + bit * 2] = static_cast<byte>((majority[bit] ? 4200 : 1950) / RAWSIGNAL_SAMPLE_RATE);
    RawSignal.Pulses[3 + bit * 2] = static_cast<byte>(480 / RAWSIGNAL_SAMPLE_RATE);
  }
  RawSignal.Pulses[74] = static_cast<byte>(SIGNAL_END_TIMEOUT_US / RAWSIGNAL_SAMPLE_RATE);
  return true;
}

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

namespace {
constexpr size_t LEGACY_PLUGIN_COUNT = sizeof(RX_PLUGINS) / sizeof(RX_PLUGINS[0]);
static uint8_t active_legacy_indices[LEGACY_PLUGIN_COUNT]{};
static uint8_t active_legacy_count = 0;
#if RFLINK_PROFILE_EXTENDED
constexpr size_t EXTENSION_TABLE_COUNT = sizeof(EXT_PLUGINS) / sizeof(EXT_PLUGINS[0]);
static uint8_t active_extension_indices[EXTENSION_TABLE_COUNT]{};
static uint8_t active_extension_count = 0;
#endif

void rebuild_active_plugin_cache() {
  active_legacy_count = 0;
  for (size_t i = 0; i < LEGACY_PLUGIN_COUNT; ++i) {
    if (mask_get(static_cast<uint16_t>(RX_PLUGINS[i].id)))
      active_legacy_indices[active_legacy_count++] = static_cast<uint8_t>(i);
  }
#if RFLINK_PROFILE_EXTENDED
  active_extension_count = 0;
  for (size_t i = 0; i < EXTENSION_TABLE_COUNT; ++i) {
    if (EXT_PLUGINS[i].decode == nullptr) break;
    if (mask_get(static_cast<uint16_t>(EXT_PLUGINS[i].id)))
      active_extension_indices[active_extension_count++] = static_cast<uint8_t>(i);
  }
#endif
}
}  // namespace

void reset(bool enable_all_compiled) {
  RawSignal = RawSignalStruct{};
  for (auto &word : plugin_enabled_mask) word = 0;

  // Plugin 001 is the mandatory RFLink packet preprocessor and must always
  // stay active when it is compiled. In managed/plugin-switch mode all other
  // decoders start OFF and are restored by their ESPHome switches.
  if (is_plugin_compiled(1)) mask_set(1, true);
  if (enable_all_compiled) {
    for (const auto id : RFLINK_COMPILED_PLUGIN_IDS) {
      const auto plugin_id = static_cast<uint16_t>(id);
      if (plugin_id == 254) continue;  // debug fallback is opt-in only
      mask_set(plugin_id, true);
    }
  }

  // Plugin 254 has a second legacy gate in the original source. Keep both
  // legacy debug flags OFF until the runtime switch explicitly enables it.
  RFUDebug = false;
  QRFUDebug = false;
  reset_repeat_history();
  reset_alecto_repeat();
  sequence = 0; clear_message(); output.reserve(256);
  rebuild_active_plugin_cache();
}
size_t plugin_count() { return RFLINK_TOTAL_PLUGINS; }
const char *plugin_profile() { return RFLINK_PLUGIN_PROFILE; }

void reset_repeat_history() {
  SignalCRC = 0;
  SignalCRC_1 = 0;
  RepeatingTimer = 0;
  SignalHash = 0;
  SignalHashPrevious = 255;
#if RFLINK_PROFILE_EXTENDED
  rf_ext::reset_history();
#endif
}

bool is_plugin_compiled(uint16_t plugin_id) {
  for (const auto id : RFLINK_COMPILED_PLUGIN_IDS)
    if (id == plugin_id) return true;
  return false;
}

bool is_plugin_enabled(uint16_t plugin_id) {
  return is_plugin_compiled(plugin_id) && mask_get(plugin_id);
}

bool set_plugin_enabled(uint16_t plugin_id, bool enabled) {
  if (!is_plugin_compiled(plugin_id)) return false;
  // 001 is required by the legacy pipeline and cannot be disabled at runtime.
  if (plugin_id == 1 && !enabled) return false;

  mask_set(plugin_id, enabled);
  if (plugin_id == 30) reset_alecto_repeat();
  if (plugin_id == 254) {
    // Make the runtime switch actually activate/deactivate the original
    // unsupported-packet analyzer. Use the readable microsecond output mode.
    RFUDebug = enabled;
    QRFUDebug = false;
  }
  // A runtime OFF/ON cycle is also an explicit recovery action. Clear stale
  // duplicate/repeat state so the first fresh packet is never suppressed by
  // history left behind by an earlier session.
  reset_repeat_history();
  rebuild_active_plugin_cache();
  return true;
}

size_t enabled_plugin_count() {
  size_t count = 0;
  for (const auto id : RFLINK_COMPILED_PLUGIN_IDS)
    if (mask_get(static_cast<uint16_t>(id))) ++count;
  return count;
}

std::string enabled_plugins_csv() {
  std::string result;
  result.reserve(RFLINK_TOTAL_PLUGINS * 4);
  char item[8];
  for (const auto id : RFLINK_COMPILED_PLUGIN_IDS) {
    if (!mask_get(static_cast<uint16_t>(id))) continue;
    if (!result.empty()) result += ',';
    std::snprintf(item, sizeof(item), "%03u", static_cast<unsigned>(id));
    result += item;
  }
  return result;
}

bool diagnose_alecto_v1_candidate(const std::vector<int32_t> &timings, std::string &summary) {
  summary.clear();
  if (timings.empty()) return false;

  size_t first = 0, end = timings.size();
  while (first < end && timings[first] < 0) ++first;
  if (first == end) return false;
  if (end > first && timings[end - 1] <= -SIGNAL_END_TIMEOUT_US) --end;
  if (end == first) return false;
  const bool append_timeout = timings[end - 1] > 0;
  const size_t count = end - first + (append_timeout ? 1 : 0);
  if (count != 74) return false;

  // Store the exact values seen by Plugin_030 after the legacy 32 us
  // quantisation. Indexing deliberately mirrors RawSignal.Pulses (1..74).
  uint16_t pulse_ticks[75]{};
  size_t dest = 1;
  for (size_t pos = first; pos < end; ++pos) {
    const int64_t signed_value = timings[pos];
    const uint64_t us = signed_value < 0 ? -signed_value : signed_value;
    if (us == 0 || us > std::numeric_limits<int32_t>::max()) {
      summary = "AlectoV1 candidate: reject=invalid pulse";
      return true;
    }
    if (((pos - first) % 2 == 0) != (signed_value > 0)) {
      char b[96];
      std::snprintf(b, sizeof(b), "AlectoV1 candidate: reject=non-alternating pulse %u",
                    static_cast<unsigned>(dest));
      summary = b;
      return true;
    }
    const uint64_t ticks = us / RAWSIGNAL_SAMPLE_RATE;
    if (ticks == 0) {
      char b[96];
      std::snprintf(b, sizeof(b), "AlectoV1 candidate: reject=sub-%uus pulse %u",
                    static_cast<unsigned>(RAWSIGNAL_SAMPLE_RATE), static_cast<unsigned>(dest));
      summary = b;
      return true;
    }
    pulse_ticks[dest++] = static_cast<uint16_t>(std::min<uint64_t>(ticks, 255));
  }
  if (append_timeout) pulse_ticks[dest++] = SIGNAL_END_TIMEOUT_US / RAWSIGNAL_SAMPLE_RATE;
  if (dest - 1 != 74) {
    summary = "AlectoV1 candidate: reject=normalisation count";
    return true;
  }

  constexpr uint16_t ALECTO_MIDHI_TICKS = 700 / RAWSIGNAL_SAMPLE_RATE;
  constexpr uint16_t ALECTO_BIT_TICKS = 2560 / RAWSIGNAL_SAMPLE_RATE;
  uint32_t bitstream = 0;
  for (uint8_t x = 2; x <= 64; x += 2) {
    if (pulse_ticks[x + 1] > ALECTO_MIDHI_TICKS) {
      char b[160];
      std::snprintf(b, sizeof(b),
                    "AlectoV1 candidate: reject=separator pulse %u is %uus (> %uus)",
                    static_cast<unsigned>(x + 1),
                    static_cast<unsigned>(pulse_ticks[x + 1] * RAWSIGNAL_SAMPLE_RATE),
                    static_cast<unsigned>(ALECTO_MIDHI_TICKS * RAWSIGNAL_SAMPLE_RATE));
      summary = b;
      return true;
    }
    bitstream >>= 1;
    if (pulse_ticks[x] > ALECTO_BIT_TICKS) bitstream |= (0x1UL << 31);
  }

  uint8_t checksum = 0;
  for (uint8_t x = 66; x <= 72; x += 2) {
    checksum >>= 1;
    if (pulse_ticks[x] > ALECTO_BIT_TICKS) checksum |= (0x1U << 3);
  }
  if (bitstream == 0) {
    summary = "AlectoV1 candidate: reject=zero bitstream";
    return true;
  }

  uint8_t data[8]{};
  uint8_t checksumcalc = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    data[i] = static_cast<uint8_t>((bitstream >> (4 * i)) & 0xF);
    checksumcalc = static_cast<uint8_t>(checksumcalc + data[i]);
  }
  if ((data[2] & 0x06) != 0x06)
    checksumcalc = static_cast<uint8_t>((0xF - checksumcalc) & 0xF);
  else if ((data[3] & 0x07) == 0x03)
    checksumcalc = static_cast<uint8_t>((0x7 + checksumcalc) & 0xF);
  else
    checksumcalc = static_cast<uint8_t>((0xF - checksumcalc) & 0xF);

  const uint8_t rc = static_cast<uint8_t>((data[1] << 4) | data[0]);
  const unsigned display_id = static_cast<unsigned>(((rc & 0x03) << 2) | (rc & 0xFC));

  if (checksum != checksumcalc) {
    char b[192];
    std::snprintf(b, sizeof(b),
                  "AlectoV1 candidate: ID=%04X; reject=checksum got=%X expected=%X; bits=%08lX",
                  display_id, static_cast<unsigned>(checksum), static_cast<unsigned>(checksumcalc),
                  static_cast<unsigned long>(bitstream));
    summary = b;
    return true;
  }

  const bool temperature_packet = (data[2] & 0x06) != 0x06;
  if (temperature_packet) {
    const uint8_t d3 = static_cast<uint8_t>(data[3] & 0x07);
    int temperature = static_cast<int>((data[5] << 8) | (data[4] << 4) | d3);
    const uint8_t humidity = static_cast<uint8_t>((data[7] << 4) | data[6]);
    if ((temperature & 0x800) != 0) {
      const int magnitude = 4096 - temperature;
      if (magnitude > 0x258) {
        char b[160];
        std::snprintf(b, sizeof(b), "AlectoV1 candidate: ID=%04X; reject=temp -%d.%dC out of range",
                      display_id, magnitude / 10, magnitude % 10);
        summary = b;
        return true;
      }
      if (humidity > 0x99) {
        char b[160];
        std::snprintf(b, sizeof(b), "AlectoV1 candidate: ID=%04X; TEMP=-%d.%dC; reject=HUM BCD %02X",
                      display_id, magnitude / 10, magnitude % 10, static_cast<unsigned>(humidity));
        summary = b;
        return true;
      }
      char b[192];
      std::snprintf(b, sizeof(b),
                    "AlectoV1 candidate: ID=%04X; TEMP=-%d.%dC; HUM_BCD=%02X; checksum=OK; would pass Plugin_030",
                    display_id, magnitude / 10, magnitude % 10, static_cast<unsigned>(humidity));
      summary = b;
      return true;
    }

    if (temperature > 0x258) {
      char b[160];
      std::snprintf(b, sizeof(b), "AlectoV1 candidate: ID=%04X; reject=temp %d.%dC out of range",
                    display_id, temperature / 10, temperature % 10);
      summary = b;
      return true;
    }
    if (humidity > 0x99) {
      char b[160];
      std::snprintf(b, sizeof(b), "AlectoV1 candidate: ID=%04X; TEMP=%d.%dC; reject=HUM BCD %02X",
                    display_id, temperature / 10, temperature % 10, static_cast<unsigned>(humidity));
      summary = b;
      return true;
    }
    char b[192];
    std::snprintf(b, sizeof(b),
                  "AlectoV1 candidate: ID=%04X; TEMP=%d.%dC; HUM_BCD=%02X; checksum=OK; would pass Plugin_030",
                  display_id, temperature / 10, temperature % 10, static_cast<unsigned>(humidity));
    summary = b;
    return true;
  }

  char b[192];
  std::snprintf(b, sizeof(b),
                "AlectoV1 candidate: ID=%04X; packet=rain/wind; checksum=OK; would pass Plugin_030",
                display_id);
  summary = b;
  return true;
}

bool decode(const std::vector<int32_t> &timings, std::string &json, FrameObservation *observation,
            UnsupportedObservation *unsupported) {
  if (observation != nullptr) *observation = FrameObservation{};
  if (unsupported != nullptr) *unsupported = UnsupportedObservation{};
  json.clear(); clear_message(); RawSignal = RawSignalStruct{};
  if (timings.empty()) return false;
#if RFLINK_PROFILE_EXTENDED
  // Most installations use only legacy decoders at runtime. Avoid validating
  // and walking the complete microsecond pulse view when no extension decoder
  // is enabled. Runtime switch changes rebuild this tiny active-index cache.
  if (active_extension_count != 0) {
    const rf_ext::Pulses pulses(timings);
    if (!pulses.valid) return false;
    for (size_t active = 0; active < active_extension_count; ++active) {
      const auto &extension = EXT_PLUGINS[active_extension_indices[active]];
      if (extension.decode(pulses)) {
        if (finished && !overflow) json = output;
        return true;
      }
    }
    clear_message();
  }
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
  // The legacy Plugin 254 intentionally accepts packets from 24 pulses.
  // Ordinary legacy decoders still keep the historical MIN_RAW_PULSES gate.
  const bool short_debug_only = count < MIN_RAW_PULSES && count >= 24 && mask_get(254);
  if ((count < MIN_RAW_PULSES && !short_debug_only) || count > RAW_BUFFER_SIZE - 1) return false;
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
  for (size_t active = 0; active < active_legacy_count; ++active) {
    const size_t index = active_legacy_indices[active];
    if (short_debug_only && RX_PLUGINS[index].id != 254) continue;
    // Keep SignalHash tied to the original registry index, not the compact
    // active-list index, preserving legacy repeat/hash behaviour.
    SignalHash = static_cast<byte>(index);
    // Plugin 254 clears RawSignal.Number before returning. Preserve the count
    // so a bounded copy can be published to HA after the fallback accepts it.
    const int raw_count_before = RX_PLUGINS[index].id == 254 ? RawSignal.Number : 0;
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
      if (RX_PLUGINS[index].id == 254) {
        build_unsupported_summary(raw_count_before, unsupported);
      }
      SignalHashPrevious = SignalHash;
      RepeatingTimer = millis() + SIGNAL_REPEAT_TIME_MS;
      if (finished && !overflow) json = output;
      return true;  // includes duplicates deliberately suppressed by the plugin
    }
    if (RX_PLUGINS[index].id == 30) {
      const RawSignalStruct original = RawSignal;
      if (prepare_alecto_repeat_recovery(timings)) {
        clear_message();
        SignalHash = static_cast<byte>(index);
        if (RX_PLUGINS[index].decode(0, nullptr)) {
          reset_alecto_repeat();
          SignalHashPrevious = SignalHash;
          RepeatingTimer = millis() + SIGNAL_REPEAT_TIME_MS;
          if (finished && !overflow) json = output;
          return true;
        }
        RawSignal = original;
        clear_message();
      }
    }
  }
  return false;
}
}  // namespace rflink_legacy
