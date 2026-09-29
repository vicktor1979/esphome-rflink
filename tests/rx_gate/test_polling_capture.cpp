// Actual receiver -> bridge -> original plugins -> production EV gesture state.
// GPIO reads and time are deterministic test doubles. No RF/Wi-Fi emulation.
#include "remote_receiver.h"
#include "rflink.h"
#include "rflink_engine.h"
#include <Arduino.h>
#include <cassert>
#include <iostream>
#include <map>
#include <utility>

uint32_t capture_clock = 0, test_millis = 0;
static uint64_t clock_us = 1000000;
static std::vector<std::pair<uint64_t, bool>> edges;
static size_t edge_index = 0;
static bool input_level = false;
uint32_t polling_test_micros() {
  ++clock_us;
  capture_clock = static_cast<uint32_t>(clock_us);
  test_millis = static_cast<uint32_t>(clock_us / 1000U);
  return capture_clock;
}
bool polling_test_level() {
  while (edge_index < edges.size() && edges[edge_index].first <= clock_us)
    input_level = edges[edge_index++].second;
  return input_level;
}
using Raw = std::vector<int32_t>;
static Raw ev(uint32_t code = 0x1fac28U) {
  Raw raw;
  for (int bit = 23; bit >= 0; --bit) {
    bool one = (code >> bit) & 1U;
    raw.insert(raw.end(), {one ? 900 : 300, one ? -300 : -900});
  }
  raw.insert(raw.end(), {300, -10000});
  return raw;
}
static Raw alecto(bool bad_checksum = false) {
  uint32_t payload = 0x75U | (1U << 8) | (225U << 12) | (0x50U << 24);
  unsigned sum = 0;
  for (unsigned n = 0; n < 8; ++n) sum += (payload >> (n * 4U)) & 15U;
  unsigned check = (15U - sum) & 15U;
  if (bad_checksum) check ^= 1U;
  Raw raw;
  for (unsigned bit = 0; bit < 36; ++bit) {
    bool one = bit < 32 ? ((payload >> bit) & 1U) : ((check >> (bit - 32)) & 1U);
    raw.insert(raw.end(), {498, one ? -4506 : -1944});
  }
  raw.insert(raw.end(), {498, -10000});
  return raw;
}
static uint64_t add(uint64_t start, const Raw &raw) {
  for (int32_t width : raw) {
    edges.emplace_back(start, width > 0);
    start += std::abs(width);
  }
  return start;
}
static void timeline(uint64_t start = 0) {
  if (start) clock_us = start;
  polling_test_micros();
  edges.clear(); edge_index = 0; input_level = false;
  rflink_legacy::reset_repeat_history();
}

int main() {
  esphome::InternalGPIOPin pin;
  esphome::remote_receiver::RemoteReceiverComponent rx(&pin);
  esphome::rflink::RFLinkComponent bridge;
  esphome::rflink_gestures::Button button;
  std::map<std::string, unsigned> events;
  esphome::rflink_gestures::Timing timing;
  timing.hold_release_ms = 450; timing.repeat_fresh_ms = 180;
  auto clear_button = [&] {
    events.clear();
    assert(button.configure_ev1527("01fac2", "08", timing));
    button.set_callback([&](const esphome::rflink_gestures::Event &event) { ++events[event.type]; });
  };
  clear_button();
  rx.set_buffer_size(1200); rx.set_filter_us(100); rx.set_idle_us(5000);
  rx.set_rflink_polling(true); rx.set_high_frequency(false); rx.set_capture_enabled(false);
  bridge.set_plugin_switch_mode(true); bridge.set_log_messages(false); bridge.setup();
  bridge.set_plugin_enabled(30, true); bridge.set_plugin_enabled(61, true);
  rx.register_listener(&bridge);
  unsigned alecto_messages = 0, ev_messages = 0;
  bridge.add_on_message_callback([&](std::string message) {
    if (message.find("Alecto V1") != std::string::npos) {
      ++alecto_messages;
      assert(message.find("\"TEMP\":\"00e1\"") != std::string::npos);
    }
    if (message.find("EV1527") != std::string::npos) ++ev_messages;
  });
  bridge.add_on_frame_callback([&](uint16_t plugin, uint32_t code) {
    button.observe(plugin, code, test_millis);
  });
  rx.setup();
  const auto before_gate = clock_us;
  rx.loop();
  assert(clock_us == before_gate && !pin.attached() && !rx.is_high_frequency_requested());
  rx.set_capture_enabled(true);
  assert(!pin.attached() && rx.is_high_frequency_requested());
  uint64_t max_loop = 0;
  auto run_until = [&](uint64_t end, uint32_t other_work_us = 1500) {
    while (clock_us < end) {
      const auto begin = clock_us;
      rx.loop();
      max_loop = std::max(max_loop, clock_us - begin);
      assert(clock_us - begin <= 225010U);
      button.tick(test_millis);
      clock_us += other_work_us;  // simulated work outside the receiver
      polling_test_micros();
    }
  };

  timeline(); auto end = add(clock_us + 2000, alecto());
  run_until(end + 30000);
  assert(alecto_messages == 1);
  timeline(); end = add(clock_us + 2000, alecto(true)); run_until(end + 30000);
  assert(alecto_messages == 1);
  std::cout << "PASS polling GPIO -> original Alecto plugin -> 22.5 C; bad checksum rejected\n";

  timeline(); clear_button();
  end = clock_us + 2000;
  for (unsigned i = 0; i < 4; ++i) end = add(end, ev());
  const auto observed = bridge.get_observed_frames();
  run_until(end + 800000);
  assert(bridge.get_observed_frames() == observed + 4 && ev_messages == 1);
  assert(events["press"] == 1 && events["single"] == 1 && events["hold"] == 0);
  std::cout << "PASS all 4 EV repeats observed; original duplicate JSON suppression; one single click\n";

  timeline(); clear_button(); end = clock_us + 2000;
  for (unsigned i = 0; i < 3; ++i) end = add(end, ev());
  end += 220000;
  for (unsigned i = 0; i < 3; ++i) end = add(end, ev());
  run_until(end + 800000);
  assert(events["double"] == 1 && events["single"] == 0);
  std::cout << "PASS EV double click through actual polling and decoder\n";

  timeline(); clear_button(); end = clock_us + 2000;
  for (unsigned i = 0; i < 30; ++i) end = add(end, ev());
  end = add(end, alecto());
  for (unsigned i = 0; i < 30; ++i) end = add(end, ev());
  const auto before_alecto = alecto_messages;
  run_until(end + 800000);
  assert(alecto_messages == before_alecto + 1);
  assert(events["press"] == 1 && events["hold"] == 1 && events["hold_repeat"] > 0);
  assert(events["hold_release"] == 1 && events["single"] == 0);
  std::cout << "PASS EV hold + interleaved Alecto + continued EV = one hold/release, no false click\n";

  // Original scanner abandons a sub-100 us glitch, then resynchronizes.
  timeline(); end = add(clock_us + 2000, {500, -1900, 40, -10000});
  end = add(end, ev()); const auto short_count = rx.get_polling_short_rejects();
  const auto ev_before = bridge.get_observed_frames();
  run_until(end + 800000);
  assert(rx.get_polling_short_rejects() > short_count);
  assert(bridge.get_observed_frames() == ev_before + 1);
  std::cout << "PASS short-glitch abort resynchronizes and next EV packet decodes\n";

  // A continuous input cannot block the scheduler indefinitely.
  timeline(); Raw noise;
  for (unsigned i = 0; i < 2000; ++i) noise.insert(noise.end(), {110, -410});
  end = add(clock_us + 2000, noise);
  const auto limits = rx.get_polling_limit_rejects();
  run_until(end + 10000);
  assert(rx.get_polling_limit_rejects() > limits);
  timeline(); noise.clear();
  for (unsigned i = 0; i < 90; ++i) noise.insert(noise.end(), {4900, -4900});
  end = add(clock_us + 2000, noise); run_until(end + 10000);
  assert(max_loop <= 225010);
  std::cout << "PASS noise and long continuous pulses bounded; max loop us=" << max_loop << '\n';

  // Microsecond timer wrap is normal after roughly 71 minutes.
  timeline(UINT64_C(0xFFFFFFFF) - 10000); clear_button();
  const auto wrap_before = bridge.get_observed_frames();
  end = add(clock_us + 2000, ev()); run_until(end + 800000);
  assert(bridge.get_observed_frames() == wrap_before + 1 && events["single"] == 1);
  std::cout << "PASS capture across micros() wrap\n";

  rx.set_capture_enabled(false);
  assert(!pin.attached() && !rx.is_high_frequency_requested());
  timeline(); end = add(clock_us + 2000, ev());
  auto calls = bridge.get_decode_calls();
  rx.loop(); assert(bridge.get_decode_calls() == calls);
  rx.set_capture_enabled(true); run_until(end + 800000);
  assert(bridge.get_decode_calls() > calls);
  rx.on_shutdown(); assert(!rx.is_high_frequency_requested());
  std::cout << "PASS polling gate, resume and shutdown; no GPIO ISR ever installed\n";
  std::cout << "LIMIT host waveforms and scheduler fixture, not RF/Wi-Fi hardware validation\n";
}
