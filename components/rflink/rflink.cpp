// Runtime diagnostic/plugin gate; does not modify the original RFLink plugins.
#include "rflink.h"
#include "rflink_engine.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rflink {
static const char *const TAG = "rflink";

void RFLinkComponent::setup() {
  // With plugin_switches configured, only the mandatory preprocessor (001)
  // starts enabled. Individual switch components then restore/choose the
  // runtime states. Without plugin_switches, keep the pre-v0.1.8 behaviour:
  // all ordinary compiled decoders start enabled (254 debug stays OFF).
  ::rflink_legacy::reset(!this->plugin_switch_mode_);
  // Typical RFLink packets are well below this size. Reserve once so the hot
  // decode/callback path does not repeatedly grow the std::string heap buffer.
  this->message_buffer_.reserve(256);
  this->publish_active_plugins();
}

void RFLinkComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RFLink RX compatibility bridge v0.1.9 (optimized active dispatch; managed runtime gates):");
  ESP_LOGCONFIG(TAG, "  Plugin profile: %s", rflink_legacy::plugin_profile());
  ESP_LOGCONFIG(TAG, "  RX plugins compiled: %u", static_cast<unsigned>(::rflink_legacy::plugin_count()));
  ESP_LOGCONFIG(TAG, "  RX plugins enabled: %u", static_cast<unsigned>(::rflink_legacy::enabled_plugin_count()));
  ESP_LOGCONFIG(TAG, "  Decode enabled: %s", this->decode_enabled_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Arduino framework; original archive preserved; optional audited overrides; TX not implemented");
}

void RFLinkComponent::set_decode_enabled(bool enabled) {
  if (this->decode_enabled_ == enabled) return;
  this->decode_enabled_ = enabled;
  ESP_LOGI(TAG, "RFLink decode %s (RF capture remains active)", enabled ? "ON" : "OFF");
  this->decode_state_callbacks_.call(enabled);
}

bool RFLinkComponent::is_plugin_compiled(uint16_t plugin_id) const {
  return ::rflink_legacy::is_plugin_compiled(plugin_id);
}

bool RFLinkComponent::is_plugin_enabled(uint16_t plugin_id) const {
  return ::rflink_legacy::is_plugin_enabled(plugin_id);
}

size_t RFLinkComponent::get_enabled_plugin_count() const {
  return ::rflink_legacy::enabled_plugin_count();
}

bool RFLinkComponent::set_plugin_enabled(uint16_t plugin_id, bool enabled) {
  if (!::rflink_legacy::set_plugin_enabled(plugin_id, enabled)) {
    ESP_LOGW(TAG, "Plugin %03u is not compiled; runtime state unchanged", static_cast<unsigned>(plugin_id));
    return false;
  }
  ESP_LOGI(TAG, "Plugin %03u runtime %s", static_cast<unsigned>(plugin_id), enabled ? "ON" : "OFF");
  this->publish_active_plugins();
  return true;
}

void RFLinkComponent::publish_active_plugins() {
  if (this->active_plugins_text_sensor_ == nullptr) return;
  this->active_plugins_text_sensor_->publish_state(::rflink_legacy::enabled_plugins_csv());
}

void RFLinkPluginSwitch::setup() {
  auto initial = this->get_initial_state_with_restore_mode();
  const bool enabled = initial.has_value() ? *initial : true;
  if (!this->parent_->set_plugin_enabled(this->plugin_id_, enabled)) {
    this->mark_failed();
    return;
  }
  this->publish_state(enabled);
}

void RFLinkPluginSwitch::dump_config() {
  LOG_SWITCH("  ", "RFLink plugin", this);
  ESP_LOGCONFIG(TAG, "    Plugin ID: %03u", static_cast<unsigned>(this->plugin_id_));
}

void RFLinkPluginSwitch::write_state(bool state) {
  if (this->parent_->set_plugin_enabled(this->plugin_id_, state)) this->publish_state(state);
}

bool RFLinkComponent::on_receive(remote_base::RemoteReceiveData data) {
  if (!this->decode_enabled_) {
    ++this->skipped_frames_;
    return false;
  }
  ++this->decode_calls_;
  this->message_buffer_.clear();
  const uint32_t decode_start = micros();
  ::rflink_legacy::FrameObservation observation;
  ::rflink_legacy::UnsupportedObservation unsupported;
  const bool recognized = ::rflink_legacy::decode(data.get_raw_data(), this->message_buffer_, &observation, &unsupported);
  const uint32_t decode_us = static_cast<uint32_t>(micros() - decode_start);
  if (decode_us > this->max_decode_us_) this->max_decode_us_ = decode_us;
  if (observation.valid) {
    ++this->observed_frames_;
    const uint32_t frame_start = micros();
    this->frame_callbacks_.call(observation.plugin_id, observation.code);
    const uint32_t frame_us = static_cast<uint32_t>(micros() - frame_start);
    if (frame_us > this->max_frame_callback_us_) this->max_frame_callback_us_ = frame_us;
  }
  if (unsupported.valid) {
    if (this->unsupported_signal_text_sensor_ != nullptr)
      this->unsupported_signal_text_sensor_->publish_state(unsupported.summary);
    if (this->unsupported_pulse_count_sensor_ != nullptr)
      this->unsupported_pulse_count_sensor_->publish_state(unsupported.pulse_count);
    if (this->log_messages_)
      ESP_LOGD(TAG, "Plugin 254 unsupported RF: %s%s", unsupported.summary.c_str(),
               unsupported.truncated ? " [HA summary truncated]" : "");
  }
  if (!this->message_buffer_.empty()) {
    ++this->message_count_;
    if (this->log_messages_) ESP_LOGD(TAG, "%s", this->message_buffer_.c_str());
    const uint32_t callback_start = micros();
    // Internal observers get a const reference; user automations keep the
    // existing by-value ABI and therefore pay a copy only when configured.
    this->message_observers_.call(this->message_buffer_);
    this->callbacks_.call(this->message_buffer_);
    const uint32_t callback_us = static_cast<uint32_t>(micros() - callback_start);
    if (callback_us > this->max_callback_us_) this->max_callback_us_ = callback_us;
  }
  return recognized;
}
}  // namespace rflink
}  // namespace esphome
