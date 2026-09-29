// Host regressions for v0.2.0.6. No RF hardware or ESP8266 timing emulation.
#include "rflink.h"
#include "rflink_engine.h"
#include <Arduino.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

uint32_t test_millis = 1000;
uint32_t probe_clock = 0;
uint32_t test_clock_us = 0;
uint32_t test_micros_step = 0;
using Raw = std::vector<int32_t>;

static Raw alecto(uint8_t code = 0x75, bool bad_checksum = false) {
  // LSB-first payload: 22.5 C, humidity 50, battery LOW.
  const uint32_t payload = code | (1U << 8) | (225U << 12) | (0x50U << 24);
  unsigned sum = 0;
  for (unsigned i = 0; i < 8; ++i) sum += (payload >> (4U * i)) & 15U;
  unsigned checksum = (15U - sum) & 15U;
  if (bad_checksum) checksum ^= 1U;
  Raw raw;
  for (unsigned bit = 0; bit < 36; ++bit) {
    bool one = bit < 32 ? ((payload >> bit) & 1U) : ((checksum >> (bit - 32)) & 1U);
    raw.push_back(480); raw.push_back(one ? -4512 : -1952);
  }
  raw.push_back(480); raw.push_back(-5000);
  return raw;
}

static Raw glitches(Raw raw, unsigned first_bit, unsigned bits, int32_t spike = 100) {
  for (int bit = static_cast<int>(first_bit + bits) - 1; bit >= static_cast<int>(first_bit); --bit) {
    const size_t index = 2U * bit + 1U;
    const int32_t width = -raw[index];
    raw[index] = -800;
    raw.insert(raw.begin() + index + 1, {spike, -(width - 800 - spike)});
  }
  return raw;
}

static Raw join(const std::vector<Raw> &rows) {
  Raw raw;
  for (size_t i = 0; i < rows.size(); ++i) {
    raw.insert(raw.end(), rows[i].begin(), rows[i].end() - 1);
    if (i + 1 < rows.size()) {
      // An 8500 us idle broken by a 100 us false mark: no captured interval
      // reaches the receiver's 5000 us idle threshold until deglitching.
      raw.insert(raw.end(), {-4200, 100, -4200});
    }
  }
  raw.push_back(-5000);
  return raw;
}

static Raw ev1527() {
  Raw raw;
  for (int i = 23; i >= 0; --i) {
    const bool one = (0x853728U >> i) & 1U;
    raw.push_back(one ? 900 : 300); raw.push_back(one ? -300 : -900);
  }
  raw.insert(raw.end(), {300, -5000});
  return raw;
}

static void fresh(bool debug = false) {
  rflink_legacy::reset(false);
  rflink_legacy::set_plugin_enabled(30, true);
  rflink_legacy::set_plugin_enabled(61, true);
  rflink_legacy::set_plugin_enabled(254, debug);
  test_millis += 10000;
  test_clock_us = 0; test_micros_step = 0;
}

static void assert_alecto(const Raw &raw) {
  std::string json;
  assert(rflink_legacy::decode(raw, json));
  assert(json.find("\"NAME\":\"Alecto V1\"") != std::string::npos);
  assert(json.find("\"ID\":\"0074\"") != std::string::npos);
  assert(json.find("\"TEMP\":\"00e1\"") != std::string::npos);
}

class GateProbe : public esphome::rflink::RFLinkComponent {
 public:
  uint32_t received() const { return alecto_gate_received_; }
  uint32_t published() const { return alecto_gate_published_; }
  uint8_t samples() const { return alecto_gate_samples_; }
  std::string reason() const { return alecto_gate_reason_; }
  bool filter(const std::string &message, uint32_t now) {
    message_buffer_ = message; return filter_alecto_message_(now);
  }
};

int main(int argc, char **argv) {
  std::cout << std::unitbuf;
  using namespace rflink_legacy;
  fresh(); assert_alecto(alecto());
  std::string json;
  FrameObservation ev;
  test_millis += 3000;
  assert(decode(ev1527(), json, &ev));
  assert(ev.valid && ev.code == 0x853728U && json.find("085372") != std::string::npos);
  assert(get_alecto_recovery_diagnostics().rows == 0);
  std::cout << "PASS clean Alecto and EV1527 normal path\n";

  fresh(); const auto long_single = glitches(alecto(), 0, 36);
  assert(long_single.size() == 146);
  assert_alecto(long_single);
  assert(get_alecto_recovery_diagnostics().long_blocks == 1);
  assert(get_alecto_recovery_diagnostics().normalized_pulses == 73);
  assert(get_alecto_recovery_diagnostics().clean_rows == 1);
  std::cout << "PASS 146-pulse single row restored before the 120-pulse limit\n";

  fresh(); assert_alecto(join({alecto(0x75, true), alecto()}));
  assert(get_alecto_recovery_diagnostics().split_boundaries >= 1);
  assert(get_alecto_recovery_diagnostics().rows == 2);
  std::cout << "PASS damaged delimiter split; second row survives bad first checksum\n";

  fresh();
  Raw no_boundary = alecto(); no_boundary.back() = -4500;
  const auto second = alecto(); no_boundary.insert(no_boundary.end(), second.begin(), second.end());
  assert(!decode(no_boundary, json) && json.empty());
  assert(std::string(get_alecto_recovery_diagnostics().reason) == "unsplit_long");
  std::cout << "PASS no invented boundary for a long block without a recovered idle\n";

  fresh();
  const auto damaged = glitches(alecto(), 3, 1, 300);
  assert_alecto(join({damaged, damaged, damaged}));
  assert(get_alecto_soft_frame_count() == 3);
  std::cout << "PASS non-overlapping soft repeats reach existing consensus\n";

  fresh(); test_micros_step = 10000;
  assert(!decode(join({damaged, damaged, damaged}), json));
  assert(json.empty() && get_alecto_recovery_diagnostics().budget_stops == 1);
  test_micros_step = 0;
  test_millis += 100;
  assert(decode(ev1527(), json, &ev) && ev.valid);
  std::cout << "PASS recovery budget stops extra rows; subsequent EV still decodes\n";

  fresh(); assert(!decode(join({alecto(0x75, true), alecto(0x75, true), alecto(0x75, true)}), json));
  assert(json.empty());
  fresh(); set_plugin_enabled(30, false);
  assert(!decode(long_single, json));
  assert(get_alecto_recovery_diagnostics().rows == 0);
  std::cout << "PASS bad checksum rejected; disabled Alecto stays disabled\n";

  fresh(true);
  UnsupportedObservation unsupported;
  assert(decode(join({alecto(0x75, true), alecto(0x75, true)}), json, nullptr, &unsupported));
  assert(unsupported.valid && unsupported.alecto_candidate && unsupported.pulse_count > 120);
  assert(json.find("DEBUG") != std::string::npos);
  std::cout << "PASS long failed candidate remains available to opt-in raw diagnostics\n";

  fresh();
  std::mt19937 rng(20260929U);
  for (unsigned n = 0; n < 1000; ++n) {
    Raw noise;
    const unsigned length = 60U + 2U * (rng() % 110U);
    for (unsigned i = 0; i < length - 1U; ++i) {
      const int32_t width = 100 + rng() % 4900;
      noise.push_back(i & 1U ? -width : width);
    }
    noise.push_back(-5000);
    test_millis += 4000;
    decode(noise, json);
    assert(json.find("Alecto V1") == std::string::npos);
  }
  std::cout << "PASS 1000 deterministic random captures produce no Alecto message\n";

  GateProbe component;
  component.set_plugin_switch_mode(true);
  component.add_configured_plugin(30); component.add_configured_plugin(61);
  component.setup(); component.set_plugin_enabled(30, true); component.set_plugin_enabled(61, true);
  unsigned callbacks = 0; std::string last;
  component.add_on_message_callback([&](std::string value) { ++callbacks; last = value; });
  for (unsigned i = 0; i < 3; ++i) {
    test_millis = 100000U + i * 35000U;
    component.on_receive(esphome::remote_base::RemoteReceiveData(long_single));
    assert(callbacks == (i == 2 ? 1U : 0U));
  }
  assert(component.received() == 3 && component.published() == 1);
  assert(last.find("\"SLOT\":1") != std::string::npos);
  assert(last.find("\"TEMP\":\"00E1\"") != std::string::npos);
  assert(component.reason() == "learned");
  const std::string big_jump = "{\"NAME\":\"Alecto V1\",\"ID\":\"0074\",\"TEMP\":\"0191\",\"BAT\":\"LOW\"}";
  for (unsigned i = 0; i < 4; ++i) assert(!component.filter(big_jump, 500000U + i * 35000U));
  assert(component.reason() == "large_jump");
  assert(component.filter(big_jump, 640000U));
  std::cout << "PASS end-to-end three-sample learning; large jump still requires five samples\n";

  GateProbe burst_gate;
  const std::string same = "{\"NAME\":\"Alecto V1\",\"ID\":\"0074\",\"TEMP\":\"00e1\"}";
  for (unsigned i = 0; i < 20; ++i) assert(!burst_gate.filter(same, 1000U + i * 100U));
  assert(burst_gate.samples() == 1 && burst_gate.published() == 0);
  assert(!burst_gate.filter(same, 700000U));
  assert(burst_gate.samples() == 1);
  std::cout << "PASS same RF burst cannot learn a sensor; stale candidate expires\n";

  if (argc == 2) {
    fresh();
    std::ifstream file(argv[1]); assert(file.good());
    unsigned rows = 0, emitted = 0; std::string line;
    while (std::getline(file, line)) {
      std::istringstream stream(line); stream >> test_millis;
      Raw raw; int32_t width;
      while (stream >> width) raw.push_back(width);
      decode(raw, json); ++rows;
      if (json.find("Alecto V1") != std::string::npos) ++emitted;
    }
    std::cout << "REPLAY complete logged captures=" << rows << " decoded=" << emitted
              << "; sampled log cannot reproduce missing repeats\n";
  }
}
