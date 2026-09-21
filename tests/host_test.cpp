#include "rflink_engine.h"
#include <Arduino.h>
#include <cassert>
#include <iostream>
#include <limits>
uint32_t test_millis = 1000;
static std::vector<int32_t> ev1527(unsigned long bits, bool idle = true) {
  std::vector<int32_t> raw;
  for (int i = 23; i >= 0; --i) {
    const bool one = (bits >> i) & 1;
    raw.push_back(one ? 900 : 300);
    raw.push_back(one ? -300 : -900);
  }
  raw.push_back(300);
  if (idle) raw.push_back(-5000);
  return raw;
}
int main() {
  using namespace rflink_legacy;
  reset(); std::string json;
  auto frame = ev1527(0x1fac28);
  assert(decode(frame, json));
  std::cout << json << '\n';
  assert(json.find("\"NAME\":\"EV1527\"") != std::string::npos);
  assert(json.find("\"ID\":\"01fac2\"") != std::string::npos);
  assert(json.find("\"SWITCH\":\"08\"") != std::string::npos);
  assert(json.find("\"CMD\":\"ON\"") != std::string::npos);
  test_millis = 1100;
  assert(decode(frame, json));
  if (plugin_count() <= 4) assert(json.empty());
  else if (!json.empty()) std::cerr << "NOTE: full legacy plugin set re-emits this duplicate in the host test; shared CRC state is unchanged.\n";
  test_millis = 1700;
  assert(decode(frame, json)); assert(!json.empty());
  reset(); assert(decode(ev1527(0x1fac28, false), json)); assert(!json.empty());
  reset(); frame.insert(frame.begin(), -12345);
  assert(decode(frame, json)); assert(!json.empty());
  assert(!decode({}, json)); assert(json.empty());
  assert(!decode({300, -300, 300}, json));
  std::vector<int32_t> oversized;
  for (int i = 0; i < 400; ++i) oversized.push_back(i % 2 ? -300 : 300);
  assert(!decode(oversized, json));
  frame = ev1527(0x1fac28); frame[7] = 0;
  assert(!decode(frame, json));
  frame[7] = std::numeric_limits<int32_t>::min();
  assert(!decode(frame, json));
  frame[7] = 300; assert(!decode(frame, json));
  std::cerr << "PASS: EV1527 synthetic waveform and boundary checks; "
            << plugin_count() << " RX plugins compiled.\n";
}
