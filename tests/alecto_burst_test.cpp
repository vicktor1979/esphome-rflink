// v0.2.0.8 default Plugin_030 regression; historical filename retained.
// Host stubs only, no RF hardware or ESP8266 timing emulation.
#include "rflink.h"
#include "rflink_engine.h"
#include <Arduino.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
uint32_t test_millis = 1000;
uint32_t probe_clock = 0;
using Raw = std::vector<int32_t>;
static Raw packet(uint32_t payload, bool bad_checksum = false) {
  unsigned sum = 0;
  for (unsigned i = 0; i < 8; ++i) sum += (payload >> (4U * i)) & 15U;
  const bool rain = ((payload >> 8) & 6U) == 6U && ((payload >> 12) & 7U) == 3U;
  unsigned checksum = (rain ? sum + 7U : 15U - sum) & 15U;
  if (bad_checksum) checksum ^= 1U;
  Raw raw;
  for (unsigned bit = 0; bit < 36; ++bit) {
    const bool one = bit < 32 ? ((payload >> bit) & 1U) : ((checksum >> (bit - 32)) & 1U);
    raw.push_back(480); raw.push_back(one ? -4512 : -1952);
  }
  raw.push_back(480); raw.push_back(-5000);
  return raw;
}
static Raw alecto(uint8_t code = 0x75, unsigned temp = 225, bool bad_checksum = false) {
  return packet(code | (1U << 8) | (temp << 12) | (0x50U << 24), bad_checksum);
}
static Raw ev1527() {
  Raw raw;
  for (int bit = 23; bit >= 0; --bit) {
    bool one = (0x853728U >> bit) & 1U;
    raw.push_back(one ? 900 : 300); raw.push_back(one ? -300 : -900);
  }
  raw.insert(raw.end(), {300, -5000}); return raw;
}
static void fresh(bool debug = false) {
  rflink_legacy::reset(false);
  rflink_legacy::set_plugin_enabled(30, true);
  rflink_legacy::set_plugin_enabled(61, true);
  rflink_legacy::set_plugin_enabled(254, debug);
  test_millis += 10000;
}
int main(int argc, char **argv) {
  std::cout << std::unitbuf;
  using namespace rflink_legacy;
  std::string json;
  fresh(); assert(decode(alecto(), json));
  assert(json.find("\"TEMP\":\"00e1\"") != std::string::npos);
  assert(json.find("\"ID\":\"0074\"") != std::string::npos);
  assert(json.find("\"BAT\":\"LOW\"") != std::string::npos);
  assert(json.find("SLOT") == std::string::npos && json.find("RFBASE") == std::string::npos);
  test_millis += 100; assert(decode(alecto(), json) && json.empty());
  test_millis += 3000; assert(decode(alecto(), json) && !json.empty());
  std::cout << "PASS original Alecto output and original duplicate suppression\n";

  fresh(); assert(!decode(alecto(0x75, 225, true), json) && json.empty());
  assert(!decode(alecto(0x75, 701), json) && json.empty());
  Raw damaged = alecto();
  // One extra edge pair was repairable in v0.2.0.6; now it must stay rejected.
  const int32_t space = -damaged[1]; damaged[1] = -800;
  damaged.insert(damaged.begin() + 2, {100, -(space - 900)});
  for (unsigned i = 0; i < 8; ++i) {
    test_millis += 160; assert(!decode(damaged, json) && json.empty());
  }
  Raw short_row = alecto(); short_row.erase(short_row.begin(), short_row.begin() + 12);
  assert(!decode(short_row, json) && json.empty());
  Raw joined = alecto(); joined.back() = -4200;
  const Raw second = alecto(); joined.insert(joined.end(), {100, -4200});
  joined.insert(joined.end(), second.begin(), second.end());
  assert(!decode(joined, json) && json.empty());
  std::cout << "PASS checksum/range rejection; damaged repeats are neither repaired nor combined\n";

  fresh(); set_plugin_enabled(30, false);
  assert(!decode(alecto(), json) && json.empty());
  set_plugin_enabled(30, true);
  FrameObservation ev;
  assert(decode(ev1527(), json, &ev));
  assert(ev.valid && ev.code == 0x853728U && json.find("EV1527") != std::string::npos);
  test_millis += 100; assert(decode(ev1527(), json, &ev) && json.empty() && ev.valid);
  fresh(true); UnsupportedObservation unsupported;
  assert(decode(damaged, json, nullptr, &unsupported));
  assert(unsupported.valid && unsupported.pulse_count == damaged.size());
  assert(json.find("Alecto V1") == std::string::npos);
  std::cout << "PASS plugin switch, EV1527 frame observations and Plugin 254 fallback\n";

  esphome::rflink::RFLinkComponent component;
  component.set_plugin_switch_mode(true); component.set_log_messages(false); component.setup();
  component.set_plugin_enabled(30, true);
  unsigned callbacks = 0; std::string message;
  component.add_on_message_callback([&](std::string s) { ++callbacks; message = s; });
  auto receive = [&](const Raw &raw) { return component.on_receive(esphome::remote_base::RemoteReceiveData(raw)); };
  test_millis += 10000;
  assert(receive(alecto()) && callbacks == 1);
  assert(message.find("\"TEMP\":\"00e1\"") != std::string::npos);
  assert(message.find("SLOT") == std::string::npos && message.find("CHANNEL") == std::string::npos);
  test_millis += 100; assert(receive(alecto()) && callbacks == 1);
  test_millis += 2400; assert(receive(alecto(0x75, 401)) && callbacks == 2);
  assert(message.find("\"TEMP\":\"0191\"") != std::string::npos);
  // Unknown IDs, channel-code zero and >3 transmitters pass the same plugin path.
  for (uint8_t id : {0x20, 0x31, 0x41, 0x51}) {
    test_millis += 2500; assert(receive(alecto(id)));
  }
  assert(callbacks == 6);
  test_millis += 2500;
  assert(receive(packet((0x1234U << 16) | (3U << 12) | (6U << 8) | 0x21U)));
  assert(callbacks == 7 && message.find("RAIN") != std::string::npos);
  assert(component.get_message_count() == callbacks);
  std::cout << "PASS first frame publishes immediately; no sample, five-minute, jump, channel or three-device gate\n";
  std::cout << "PASS original non-temperature Alecto message also reaches callbacks\n";
  if (argc == 2) {
    fresh(); std::ifstream input(argv[1]); assert(input.good());
    unsigned rows = 0, emitted = 0; std::string line;
    while (std::getline(input, line)) {
      std::istringstream stream(line); stream >> test_millis;
      Raw raw; int32_t width; while (stream >> width) raw.push_back(width);
      decode(raw, json); ++rows;
      if (json.find("Alecto V1") != std::string::npos) ++emitted;
    }
    std::cout << "REPLAY complete logged captures=" << rows << " decoded=" << emitted << "; original Plugin 030 only\n";
  }
}
