// Runtime diagnostic gate; does not modify the original RFLink plugins or engine.
#include "rflink.h"
#include "rflink_engine.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rflink {
static const char *const TAG = "rflink";
void RFLinkComponent::setup() { ::rflink_legacy::reset(); }
void RFLinkComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RFLink RX compatibility bridge v0.1.2-diag (runtime decode gate):");
  ESP_LOGCONFIG(TAG, "  RX plugins compiled: %u", static_cast<unsigned>(::rflink_legacy::plugin_count()));
  ESP_LOGCONFIG(TAG, "  Decode enabled: %s", this->decode_enabled_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Arduino framework; original plugin sources; TX not implemented");
}
void RFLinkComponent::set_decode_enabled(bool enabled) {
  if (this->decode_enabled_ == enabled) return;
  this->decode_enabled_ = enabled;
  ESP_LOGI(TAG, "RFLink decode %s (RF capture remains active)", enabled ? "ON" : "OFF");
  // Do not reset legacy deduplication state, entities or packet counters.
}
bool RFLinkComponent::on_receive(remote_base::RemoteReceiveData data) {
  if (!this->decode_enabled_) {
    ++this->skipped_frames_;
    return false;  // No decode, JSON allocation, or on_message callback in this branch.
  }
  ++this->decode_calls_;
  std::string json;
  const uint32_t decode_start = micros();
  const bool recognized = ::rflink_legacy::decode(data.get_raw_data(), json);
  const uint32_t decode_us = static_cast<uint32_t>(micros() - decode_start);
  if (decode_us > this->max_decode_us_) this->max_decode_us_ = decode_us;
  if (!json.empty()) {
    ++this->message_count_;
    if (this->log_messages_) ESP_LOGD(TAG, "%s", json.c_str());
    const uint32_t callback_start = micros();
    // Triggered for every emitted frame, independently of text_sensor state.
    this->callbacks_.call(json);
    const uint32_t callback_us = static_cast<uint32_t>(micros() - callback_start);
    if (callback_us > this->max_callback_us_) this->max_callback_us_ = callback_us;
  }
  return recognized;
}
}  // namespace rflink
}  // namespace esphome
