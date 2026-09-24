#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rflink_legacy {
// A lightweight side channel. It reports EVERY accepted EV1527 RF frame,
// including frames intentionally suppressed by the original plugin's JSON filter.
// A valid observation is not a physical button press/release.
struct FrameObservation {
  bool valid{false};
  uint16_t plugin_id{0};
  uint32_t code{0};  // EV1527: 20-bit device ID followed by the 4-bit SWITCH.
};
void reset();
size_t plugin_count();
const char *plugin_profile();

// Runtime plugin gate. Only plugins compiled into the current firmware can be changed.
bool is_plugin_compiled(uint16_t plugin_id);
bool is_plugin_enabled(uint16_t plugin_id);
bool set_plugin_enabled(uint16_t plugin_id, bool enabled);
size_t enabled_plugin_count();
std::string enabled_plugins_csv();

// Returns recognized=true even when an RFLink plugin suppresses a duplicate.
// Only a nonempty output JSON represents an event that should be published.
bool decode(const std::vector<int32_t> &timings, std::string &json, FrameObservation *observation = nullptr);
}  // namespace rflink_legacy
