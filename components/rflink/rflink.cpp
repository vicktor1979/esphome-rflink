// Runtime diagnostic/plugin gate; does not modify the original RFLink plugins.
#include "rflink.h"
#include "rflink_engine.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#ifdef USE_RFLINK_AUTO_START
#include "esphome/components/remote_receiver/remote_receiver.h"
#ifdef USE_NETWORK
#include "esphome/components/network/util.h"
#endif
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#ifdef USE_ESP8266
#include <Esp.h>
#endif
#endif

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

#ifdef USE_RFLINK_AUTO_START
  if (this->auto_start_enabled_) {
    // Store capture=OFF even when the receiver's own setup has not run yet.
    // This guarantees that no RF IRQ is installed while Wi-Fi/API are starting.
    if (this->receiver_ != nullptr) this->receiver_->set_capture_enabled(false);
    this->set_decode_enabled(false);
    this->ready_timing_ = false;
    this->auto_running_ = false;
    if (this->build_text_sensor_ != nullptr) {
      std::string build{"v0.1.9.3 · "};
      build += ::rflink_legacy::plugin_profile();
      build += " · ";
      build += std::to_string(static_cast<unsigned>(::rflink_legacy::plugin_count()));
      build += " plugin";
      this->build_text_sensor_->publish_state(build);
    }
    this->publish_health_("Indítás...");
  }
#endif
}

void RFLinkComponent::loop() {
#ifdef USE_RFLINK_AUTO_START
  if (!this->auto_start_enabled_ || this->receiver_ == nullptr) return;
  const uint32_t now = millis();
  // The old YAML interval checked once per second. 100 ms makes disconnect and
  // OTA response snappier while remaining negligible beside RF processing.
  const bool settle_due = this->ready_timing_ &&
                          static_cast<uint32_t>(now - this->ready_since_ms_) >= this->auto_start_settle_ms_;
  if (static_cast<uint32_t>(now - this->last_auto_check_ms_) < 100 && !settle_due) return;
  this->last_auto_check_ms_ = now;

  const bool network_ready = this->network_ready_();
  const bool api_ready = this->api_ready_();
  const bool ready = this->monitoring_enabled_ && !this->ota_active_ &&
                     (!this->require_network_ || network_ready) &&
                     (!this->require_api_ || api_ready);

  const uint32_t receiver_recoveries = this->receiver_->get_recovery_count();
  if (receiver_recoveries != this->last_receiver_recovery_count_) {
    this->last_receiver_recovery_count_ = receiver_recoveries;
    // Capture resync means the previous packet boundary is intentionally gone;
    // discard decoder duplicate history as part of the same self-heal.
    this->reset_runtime_history_("receiver resync");
  }

  if (!ready) {
    this->ready_timing_ = false;
    this->apply_auto_state_(false);
  } else if (!this->auto_running_ || !this->receiver_->is_capture_enabled() || !this->decode_enabled_) {
    if (!this->ready_timing_) {
      this->ready_timing_ = true;
      this->ready_since_ms_ = now;
    }
    if (static_cast<uint32_t>(now - this->ready_since_ms_) >= this->auto_start_settle_ms_)
      this->apply_auto_state_(true);
  }

  this->update_health_(network_ready, api_ready);
  if (this->diagnostics_interval_ms_ != 0 &&
      static_cast<uint32_t>(now - this->last_diagnostics_ms_) >= this->diagnostics_interval_ms_) {
    this->last_diagnostics_ms_ = now;
    this->update_diagnostics_(now, network_ready, api_ready);
  }
#else
  // Keep a real loop() override without imposing network/API dependencies on
  // legacy configurations that do not opt into auto_start.
#endif
}

void RFLinkComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RFLink RX compatibility bridge v0.1.9.3 (adaptive RX; Alecto candidate diagnostics):");
  ESP_LOGCONFIG(TAG, "  Plugin profile: %s", rflink_legacy::plugin_profile());
  ESP_LOGCONFIG(TAG, "  RX plugins compiled: %u", static_cast<unsigned>(::rflink_legacy::plugin_count()));
  ESP_LOGCONFIG(TAG, "  RX plugins enabled: %u", static_cast<unsigned>(::rflink_legacy::enabled_plugin_count()));
  ESP_LOGCONFIG(TAG, "  Decode enabled: %s", this->decode_enabled_ ? "YES" : "NO");
#ifdef USE_RFLINK_AUTO_START
  ESP_LOGCONFIG(TAG, "  Auto start: %s; settle=%lu ms; diagnostics=%lu ms", this->auto_start_enabled_ ? "YES" : "NO",
                static_cast<unsigned long>(this->auto_start_settle_ms_),
                static_cast<unsigned long>(this->diagnostics_interval_ms_));
#endif
  ESP_LOGCONFIG(TAG, "  Arduino framework; original archive preserved; optional audited overrides; TX not implemented");
}

void RFLinkComponent::set_decode_enabled(bool enabled) {
  if (this->decode_enabled_ == enabled) return;
  this->decode_enabled_ = enabled;
  if (!enabled) this->reset_runtime_history_("decode disabled");
  ESP_LOGI(TAG, "RFLink decode %s", enabled ? "ON" : "OFF");
  if (this->decode_active_sensor_ != nullptr) this->decode_active_sensor_->publish_state(enabled);
  this->decode_state_callbacks_.call(enabled);
}

void RFLinkComponent::reset_runtime_history_(const char *reason) {
  ::rflink_legacy::reset_repeat_history();
  this->repeat_history_dirty_ = false;
  ++this->repeat_history_resets_;
  if (this->log_messages_) ESP_LOGD(TAG, "RFLink repeat history reset: %s", reason);
}

void RFLinkComponent::set_monitoring_enabled(bool enabled) {
  if (this->monitoring_enabled_ == enabled) return;
  this->monitoring_enabled_ = enabled;
  this->ready_timing_ = false;
#ifdef USE_RFLINK_AUTO_START
  if (!enabled) this->apply_auto_state_(false);
#endif
}

void RFLinkComponent::set_ota_active(bool active) {
  if (this->ota_active_ == active) return;
  this->ota_active_ = active;
  this->ready_timing_ = false;
#ifdef USE_RFLINK_AUTO_START
  if (active) this->apply_auto_state_(false);
#endif
}

#ifdef USE_RFLINK_AUTO_START
bool RFLinkComponent::network_ready_() const {
#ifdef USE_NETWORK
  return network::is_connected();
#else
  return false;
#endif
}

bool RFLinkComponent::api_ready_() const {
#ifdef USE_API
  return api::global_api_server != nullptr && api::global_api_server->is_connected_with_state_subscription();
#else
  return false;
#endif
}

void RFLinkComponent::apply_auto_state_(bool enabled) {
  if (this->receiver_ == nullptr) return;
  const bool was_capture = this->receiver_->is_capture_enabled();
  const bool was_decode = this->decode_enabled_;
  if (enabled) {
    this->receiver_->set_capture_enabled(true);
    const bool capture = this->receiver_->is_capture_enabled();
    this->set_decode_enabled(capture);
    this->auto_running_ = capture && this->decode_enabled_;
  } else {
    // Stop decoding first so a frame that was already queued cannot become a
    // user event while capture is being torn down.
    this->set_decode_enabled(false);
    this->receiver_->set_capture_enabled(false);
    this->auto_running_ = false;
  }
  if (was_capture != this->receiver_->is_capture_enabled() || was_decode != this->decode_enabled_) {
    ESP_LOGI("rflink.auto", "CAPTURE=%s; DECODE=%s; monitoring=%s; OTA=%s",
             this->receiver_->is_capture_enabled() ? "ON" : "OFF",
             this->decode_enabled_ ? "ON" : "OFF",
             this->monitoring_enabled_ ? "ON" : "OFF", this->ota_active_ ? "ON" : "OFF");
  }
}

void RFLinkComponent::publish_health_(const char *state) {
  if (state == nullptr) return;
  if (this->last_health_ == state) return;
  this->last_health_ = state;
  if (this->health_text_sensor_ != nullptr) this->health_text_sensor_->publish_state(this->last_health_);
}

void RFLinkComponent::update_health_(bool network_ready, bool api_ready) {
  if (!this->monitoring_enabled_) {
    this->publish_health_("Kikapcsolva");
  } else if (this->ota_active_) {
    this->publish_health_("OTA");
  } else if (this->require_network_ && !network_ready) {
    this->publish_health_("Hálózatra vár");
  } else if (this->require_api_ && !api_ready) {
    this->publish_health_("HA API-ra vár");
  } else if (!this->auto_running_) {
    this->publish_health_("Indítás...");
  } else if (this->receiver_ != nullptr && this->receiver_->get_recovery_count() != 0) {
    std::string state{"OK · RX helyreállítás "};
    state += std::to_string(static_cast<unsigned long>(this->receiver_->get_recovery_count()));
    if (this->last_health_ != state) {
      this->last_health_ = state;
      if (this->health_text_sensor_ != nullptr) this->health_text_sensor_->publish_state(state);
    }
  } else {
    this->publish_health_("OK");
  }
}

void RFLinkComponent::update_diagnostics_(uint32_t now, bool network_ready, bool api_ready) {
  if (this->receiver_ == nullptr) return;
#ifdef USE_ESP8266
  ESP_LOGI("rflink.diag",
           "AUTO=%s; CAPTURE=%s; DECODE=%s; api_states=%s; network=%s; uptime=%lu s; heap=%u B; max_block=%u B; frag=%u%%; frames=%lu; decoded=%lu; calls=%lu; skipped=%lu; decode_max_us=%lu; callback_max_us=%lu; observed=%lu; frame_callback_max_us=%lu; irq_total=%lu; fast_loop=%s; rx_loop_calls=%lu; overflow_reports=%lu; recoveries=%lu; backlog_boosts=%lu; backlog_max=%lu; history_resets=%lu",
           this->monitoring_enabled_ ? "ON" : "OFF",
           this->receiver_->is_capture_enabled() ? "ON" : "OFF", this->decode_enabled_ ? "ON" : "OFF",
           api_ready ? "YES" : "NO", network_ready ? "CONNECTED" : "DISCONNECTED",
           static_cast<unsigned long>(now / 1000), static_cast<unsigned>(ESP.getFreeHeap()),
           static_cast<unsigned>(ESP.getMaxFreeBlockSize()), static_cast<unsigned>(ESP.getHeapFragmentation()),
           static_cast<unsigned long>(this->receiver_->get_frame_count()), static_cast<unsigned long>(this->message_count_),
           static_cast<unsigned long>(this->decode_calls_), static_cast<unsigned long>(this->skipped_frames_),
           static_cast<unsigned long>(this->max_decode_us_), static_cast<unsigned long>(this->max_callback_us_),
           static_cast<unsigned long>(this->observed_frames_), static_cast<unsigned long>(this->max_frame_callback_us_),
           static_cast<unsigned long>(this->receiver_->get_edge_count()),
           this->receiver_->is_high_frequency_requested() ? "ON" : "OFF",
           static_cast<unsigned long>(this->receiver_->get_loop_calls()),
           static_cast<unsigned long>(this->receiver_->get_overflow_reports()),
           static_cast<unsigned long>(this->receiver_->get_recovery_count()),
           static_cast<unsigned long>(this->receiver_->get_backlog_boost_count()),
           static_cast<unsigned long>(this->receiver_->get_max_completed_backlog()),
           static_cast<unsigned long>(this->repeat_history_resets_));
#else
  ESP_LOGI("rflink.diag",
           "AUTO=%s; CAPTURE=%s; DECODE=%s; api_states=%s; network=%s; frames=%lu; decoded=%lu; calls=%lu; skipped=%lu; decode_max_us=%lu; overflow_reports=%lu; recoveries=%lu; backlog_boosts=%lu; backlog_max=%lu; history_resets=%lu",
           this->monitoring_enabled_ ? "ON" : "OFF",
           this->receiver_->is_capture_enabled() ? "ON" : "OFF", this->decode_enabled_ ? "ON" : "OFF",
           api_ready ? "YES" : "NO", network_ready ? "CONNECTED" : "DISCONNECTED",
           static_cast<unsigned long>(this->receiver_->get_frame_count()), static_cast<unsigned long>(this->message_count_),
           static_cast<unsigned long>(this->decode_calls_), static_cast<unsigned long>(this->skipped_frames_),
           static_cast<unsigned long>(this->max_decode_us_),
           static_cast<unsigned long>(this->receiver_->get_overflow_reports()),
           static_cast<unsigned long>(this->receiver_->get_recovery_count()),
           static_cast<unsigned long>(this->receiver_->get_backlog_boost_count()),
           static_cast<unsigned long>(this->receiver_->get_max_completed_backlog()),
           static_cast<unsigned long>(this->repeat_history_resets_));
#endif
}
#endif  // USE_RFLINK_AUTO_START

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
  // set_plugin_enabled() clears the legacy history in the engine. Mirror that
  // state here so a later long-idle check does not treat old history as live.
  this->repeat_history_dirty_ = false;
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

void RFLinkMonitoringSwitch::setup() {
  auto initial = this->get_initial_state_with_restore_mode();
  const bool enabled = initial.has_value() ? *initial : true;
  this->parent_->set_monitoring_enabled(enabled);
  this->publish_state(enabled);
}

void RFLinkMonitoringSwitch::dump_config() { LOG_SWITCH("  ", "RFLink monitoring", this); }

void RFLinkMonitoringSwitch::write_state(bool state) {
  this->parent_->set_monitoring_enabled(state);
  this->publish_state(state);
}

bool RFLinkComponent::on_receive(remote_base::RemoteReceiveData data) {
  if (!this->decode_enabled_) {
    ++this->skipped_frames_;
    return false;
  }
  ++this->decode_calls_;

  const uint32_t now_ms = millis();
  // Legacy RFLink duplicate filters are short-lived (EV1527 is ~450 ms). If
  // the receiver has been quiet for >=1 s, stale history can never represent a
  // legitimate retransmit. Clearing it makes the first packet after a long
  // idle deterministic, and also self-heals any corrupted repeat state.
  if (this->repeat_history_dirty_ && static_cast<uint32_t>(now_ms - this->last_recognized_ms_) >= 1000) {
    this->reset_runtime_history_("long RF idle");
  }

  this->message_buffer_.clear();
  const uint32_t decode_start = micros();
  ::rflink_legacy::FrameObservation observation;
  ::rflink_legacy::UnsupportedObservation unsupported;
  const bool recognized = ::rflink_legacy::decode(data.get_raw_data(), this->message_buffer_, &observation, &unsupported);
  const uint32_t decode_us = static_cast<uint32_t>(micros() - decode_start);
  if (decode_us > this->max_decode_us_) this->max_decode_us_ = decode_us;
  if (recognized) {
    this->last_recognized_ms_ = now_ms;
    this->repeat_history_dirty_ = true;
  }
  if (observation.valid) {
    ++this->observed_frames_;
    const uint32_t frame_start = micros();
    this->frame_callbacks_.call(observation.plugin_id, observation.code);
    const uint32_t frame_us = static_cast<uint32_t>(micros() - frame_start);
    if (frame_us > this->max_frame_callback_us_) this->max_frame_callback_us_ = frame_us;
  }
  if (unsupported.valid) {
    std::string alecto_diagnostic;
    const bool alecto_candidate = unsupported.pulse_count == 74 &&
        ::rflink_legacy::diagnose_alecto_v1_candidate(data.get_raw_data(), alecto_diagnostic);

    if (this->unsupported_signal_text_sensor_ != nullptr) {
      // A 74-pulse Alecto candidate is far more useful in HA when we expose
      // the exact Plugin_030 reject reason instead of only a truncated pulse list.
      this->unsupported_signal_text_sensor_->publish_state(
          alecto_candidate ? alecto_diagnostic : unsupported.summary);
    }
    if (this->unsupported_pulse_count_sensor_ != nullptr)
      this->unsupported_pulse_count_sensor_->publish_state(unsupported.pulse_count);

    if (alecto_candidate) {
      ESP_LOGW("rflink.alecto", "%s; plugin030=%s", alecto_diagnostic.c_str(),
               ::rflink_legacy::is_plugin_enabled(30) ? "ON" : "OFF");
    }
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
