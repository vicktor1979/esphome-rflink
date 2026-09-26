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

// Captured only when the legacy Plugin 254 fallback accepts an unsupported
// packet. The summary is intentionally bounded so it is safe to publish as a
// Home Assistant text-sensor state. The exact pulse count remains separate.
struct UnsupportedObservation {
  bool valid{false};
  uint16_t pulse_count{0};
  bool truncated{false};
  std::string summary;
};
void reset(bool enable_all_compiled = true);
size_t plugin_count();
const char *plugin_profile();

// Cumulative Alecto recovery diagnostics since bridge reset. soft_frame_count
// counts damaged rows that passed the cheap Alecto waveform gate + alignment;
// reconstructed_count counts checksum/range-valid canonical rows prepared for
// the untouched Plugin_030.
uint32_t get_alecto_soft_frame_count();
uint32_t get_alecto_reconstructed_count();
uint8_t get_alecto_last_frames();
uint8_t get_alecto_last_data_strong();
uint8_t get_alecto_last_checksum_strong();

// Runtime plugin gate. Only plugins compiled into the current firmware can be changed.
bool is_plugin_compiled(uint16_t plugin_id);
bool is_plugin_enabled(uint16_t plugin_id);
bool set_plugin_enabled(uint16_t plugin_id, bool enabled);
size_t enabled_plugin_count();
std::string enabled_plugins_csv();

// Capability map generated from the untouched plugin sources at build time.
// Each bit corresponds to one RFLink display_* data field.
uint64_t field_capability_mask(const char *field);
uint64_t plugin_capability_mask(uint16_t plugin_id);
uint64_t compiled_capability_mask();
uint64_t enabled_capability_mask();
bool plugin_supports_field(uint16_t plugin_id, const char *field);
bool enabled_plugins_support_field(const char *field);

// Diagnose a legacy Alecto V1 (Plugin 030) candidate without changing or bypassing
// the original RFLink plugin. Returns true only when the normalized frame has the
// Alecto V1 pulse count (74); summary then explains the first Plugin_030 reject
// reason or reports that the frame would pass Plugin_030.
bool diagnose_alecto_v1_candidate(const std::vector<int32_t> &timings, std::string &summary);

// Clear only duplicate/repeat history; plugin enable masks and sequence stay unchanged.
void reset_repeat_history();

// Returns recognized=true even when an RFLink plugin suppresses a duplicate.
// Only a nonempty output JSON represents an event that should be published.
bool decode(const std::vector<int32_t> &timings, std::string &json, FrameObservation *observation = nullptr,
            UnsupportedObservation *unsupported = nullptr);
}  // namespace rflink_legacy
