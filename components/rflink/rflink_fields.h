#pragma once
// v0.1.3 -- helpers only. No radio, GPIO, plugin or legacy formatter changes.
// Input contract: v0.1.1 RFLink bridge JSON, NOT malformed legacy serial JSON.
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

namespace esphome { namespace rflink_data {
enum class Encoding { PLAIN, SIGN_MAGNITUDE_TENTHS };
struct FieldSpec { const char *key; uint8_t base; uint32_t max_raw; Encoding encoding; float scale; };
enum Field : size_t { SET_LEVEL, TEMP, HUM, BARO, HSTATUS, BFORECAST, UV, LUX, RAIN, RAINRATE, WINSP, AWINSP, WINGS, WINDIR, WINCHL, WINTMP, CHIME, CO2, SOUND, KWATT, WATT, CURRENT, DIST, METER, VOLT, FIELD_COUNT };
static constexpr FieldSpec FIELDS[FIELD_COUNT] = {
  {"SET_LEVEL", 10, 255u, Encoding::PLAIN, 1.0f},
  {"TEMP", 16, 65535u, Encoding::SIGN_MAGNITUDE_TENTHS, 0.1f},
  {"HUM", 10, 100u, Encoding::PLAIN, 1.0f},
  {"BARO", 16, 4294967295u, Encoding::PLAIN, 1.0f},
  {"HSTATUS", 16, 255u, Encoding::PLAIN, 1.0f},
  {"BFORECAST", 16, 255u, Encoding::PLAIN, 1.0f},
  {"UV", 16, 4294967295u, Encoding::PLAIN, 1.0f},
  {"LUX", 16, 4294967295u, Encoding::PLAIN, 1.0f},
  {"RAIN", 16, 4294967295u, Encoding::PLAIN, 0.1f},
  {"RAINRATE", 16, 4294967295u, Encoding::PLAIN, 0.1f},
  {"WINSP", 16, 4294967295u, Encoding::PLAIN, 0.1f},
  {"AWINSP", 16, 4294967295u, Encoding::PLAIN, 0.1f},
  {"WINGS", 16, 4294967295u, Encoding::PLAIN, 1.0f},
  {"WINDIR", 10, 15u, Encoding::PLAIN, 22.5f},
  {"WINCHL", 16, 65535u, Encoding::SIGN_MAGNITUDE_TENTHS, 0.1f},
  {"WINTMP", 16, 65535u, Encoding::SIGN_MAGNITUDE_TENTHS, 0.1f},
  {"CHIME", 10, 4294967295u, Encoding::PLAIN, 1.0f},
  {"CO2", 10, 4294967295u, Encoding::PLAIN, 1.0f},
  {"SOUND", 10, 4294967295u, Encoding::PLAIN, 1.0f},
  {"KWATT", 16, 4294967295u, Encoding::PLAIN, 1.0f},
  {"WATT", 16, 4294967295u, Encoding::PLAIN, 1.0f},
  {"CURRENT", 10, 4294967295u, Encoding::PLAIN, 1.0f},
  {"DIST", 10, 4294967295u, Encoding::PLAIN, 1.0f},
  {"METER", 10, 4294967295u, Encoding::PLAIN, 1.0f},
  {"VOLT", 10, 4294967295u, Encoding::PLAIN, 1.0f},
};
struct Number { bool present{false}; bool valid{false}; float value{NAN}; };

// Strict, unsigned, base-aware input. Reject trailing junk and overflow.
inline bool parse_unsigned(const char *text, uint8_t base, uint32_t limit, uint32_t &out) {
  if (text == nullptr || *text == 0 || (base != 10 && base != 16)) return false;
  uint32_t value = 0;
  for (const char *p = text; *p != 0; ++p) {
    const char c = *p;
    uint32_t digit;
    if (c >= '0' && c <= '9') digit = c - '0';
    else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
    else return false;
    if (digit >= base || digit > limit || value > (limit - digit) / base) return false;
    value = value * base + digit;
  }
  out = value;
  return true;
}

// Templated on JsonObject so the conversion is testable without RF hardware.
template<typename Object> Number number(Object root, Field field) {
  const FieldSpec &spec = FIELDS[field];
  auto token = root[spec.key];
  Number result;
  result.present = !token.isNull();
  if (!result.present) return result;
  uint32_t raw = 0;
  if (token.template is<const char *>()) {
    if (!parse_unsigned(token.template as<const char *>(), spec.base, spec.max_raw, raw)) return result;
  } else if (spec.base == 10 && !token.template is<bool>() && token.template is<uint32_t>()) {
    raw = token.template as<uint32_t>();
    if (raw > spec.max_raw) return result;
  } else {
    // Hex fields MUST remain strings: numeric 100 is not an unambiguous hex token.
    return result;
  }
  result.valid = true;
  if (spec.encoding == Encoding::SIGN_MAGNITUDE_TENTHS) {
    result.value = static_cast<float>(raw & 0x7fffU) / 10.0f;
    if ((raw & 0x8000U) != 0) result.value = -result.value;
  } else {
    result.value = static_cast<float>(raw) * spec.scale;
  }
  return result;
}

// missing=-1; explicit off=0; explicit on=1. Never invent an OFF/OK reading.
inline int binary_value(const std::string &value, const char *on, const char *off) {
  if (value == on) return 1;
  if (value == off) return 0;
  return -1;
}
inline std::string hstatus_label(const Number &n) {
  if (!n.valid) return "Nincs adat";
  const unsigned code = static_cast<unsigned>(n.value);
  switch(code) {
    case 0: return "Normál";
    case 1: return "Komfortos";
    case 2: return "Száraz";
    case 3: return "Nedves";
    default: return "Ismeretlen kód: " + std::to_string(code);
  }
}
inline std::string forecast_label(const Number &n) {
  if (!n.valid) return "Nincs adat";
  const unsigned code = static_cast<unsigned>(n.value);
  switch(code) {
    case 0: return "Nincs információ";
    case 1: return "Napos";
    case 2: return "Részben felhős";
    case 3: return "Felhős";
    case 4: return "Eső";
    default: return "Ismeretlen kód: " + std::to_string(code);
  }
}

template<typename Sensor> void publish_number(Sensor *sensor, const Number &n, bool snapshot) {
  if (!snapshot && !n.present) return;  // Separate device: retain other readings until timeout.
  if (!n.valid) {
    if (sensor->has_state() && !std::isnan(sensor->state)) sensor->publish_state(NAN);
    return;
  }
  // Fixed-device samples always refresh the expiry timer, even unchanged values.
  if (!snapshot || !sensor->has_state() || sensor->state != n.value) sensor->publish_state(n.value);
}
template<typename Sensor> void publish_binary(Sensor *sensor, int state, bool present, bool snapshot) {
  if (!snapshot && !present) return;
  if (state < 0) {
    if (sensor->has_state()) sensor->invalidate_state();
  } else if (!snapshot || !sensor->has_state() || sensor->state != (state == 1)) {
    sensor->publish_state(state == 1);
  }
}
template<typename Sensor> void publish_text(Sensor *sensor, const std::string &value) {
  if (!sensor->has_state() || sensor->state != value) sensor->publish_state(value);
}

// Five <=240-byte chunks cover the bridge's 1024-byte maximum without cutting UTF-8.
inline std::string json_part(const std::string &text, size_t part) {
  size_t start = 0;
  for (size_t i = 0; i <= part; ++i) {
    if (start >= text.size()) return "";
    size_t end = start + 240;
    if (end > text.size()) end = text.size();
    while (end < text.size() && end > start &&
           (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
    if (i == part) return text.substr(start, end - start);
    start = end;
  }
  return "";
}

// Polled after API state subscription, NOT just the early TCP/hello callback.
// Unsigned subtraction tolerates millis rollover. There is no 60-second cutoff.
class ReadyGate {
 public:
  bool update(uint32_t now, bool ready, uint32_t settle_ms) {
    if (!ready) { this->waiting_ = false; this->active_ = false; return false; }
    if (this->active_) return true;
    if (!this->waiting_) { this->waiting_ = true; this->since_ = now; }
    this->active_ = static_cast<uint32_t>(now - this->since_) >= settle_ms;
    return this->active_;
  }
  void reset() { this->waiting_ = false; this->active_ = false; }
 private:
  bool waiting_{false};
  bool active_{false};
  uint32_t since_{0};
};
}}  // namespace esphome::rflink_data
