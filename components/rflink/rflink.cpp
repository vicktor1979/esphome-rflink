// Runtime diagnostic/plugin gate; does not modify the original RFLink plugins.
#include "rflink.h"
#include "rflink_engine.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
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

namespace {
constexpr uint32_t ALECTO_SAMPLE_MIN_SPACING_MS = 5000U;
constexpr uint32_t ALECTO_CANDIDATE_TIMEOUT_MS = 10U * 60U * 1000U;
constexpr uint32_t ALECTO_SLOT_STALE_MS = 30U * 60U * 1000U;
constexpr uint32_t ALECTO_PUBLISH_INTERVAL_MS = 5U * 60U * 1000U;
constexpr int16_t ALECTO_CLUSTER_SPAN_TENTHS = 15;  // 1.5 C inside one accepted cluster.
constexpr int16_t ALECTO_LARGE_JUMP_TENTHS = 100;  // 10 C needs all five samples.

bool json_string_field(const std::string &json, const char *key, std::string &value) {
  if (key == nullptr) return false;
  std::string needle{"\""};
  needle += key;
  needle += "\":\"";
  const size_t start = json.find(needle);
  if (start == std::string::npos) return false;
  const size_t value_start = start + needle.size();
  const size_t end = json.find('"', value_start);
  if (end == std::string::npos) return false;
  value.assign(json, value_start, end - value_start);
  return true;
}

bool replace_json_string_field(std::string &json, const char *key, const std::string &value) {
  std::string needle{"\""};
  needle += key;
  needle += "\":\"";
  const size_t start = json.find(needle);
  if (start == std::string::npos) return false;
  const size_t value_start = start + needle.size();
  const size_t end = json.find('"', value_start);
  if (end == std::string::npos) return false;
  json.replace(value_start, end - value_start, value);
  return true;
}

bool parse_hex_u16(const std::string &text, uint16_t &value) {
  if (text.empty() || text.size() > 4) return false;
  char *end = nullptr;
  const unsigned long parsed = std::strtoul(text.c_str(), &end, 16);
  if (end == text.c_str() || *end != '\0' || parsed > 0xFFFFUL) return false;
  value = static_cast<uint16_t>(parsed);
  return true;
}

bool parse_rflink_temp(const std::string &text, int16_t &tenths) {
  uint16_t raw = 0;
  if (!parse_hex_u16(text, raw)) return false;
  const int16_t magnitude = static_cast<int16_t>(raw & 0x7FFFU);
  if (magnitude > 2000) return false;  // defensive only; Plugin_030 is much tighter.
  tenths = (raw & 0x8000U) != 0U ? static_cast<int16_t>(-magnitude) : magnitude;
  return true;
}

std::string format_hex4(uint16_t value) {
  char buffer[5];
  std::snprintf(buffer, sizeof(buffer), "%04X", static_cast<unsigned>(value));
  return buffer;
}

std::string format_rflink_temp(int16_t tenths) {
  uint16_t raw = static_cast<uint16_t>(tenths < 0 ? -static_cast<int32_t>(tenths) : tenths);
  if (tenths < 0) raw |= 0x8000U;
  return format_hex4(raw);
}

uint8_t alecto_channel_from_id(uint16_t id) {
  // Plugin_030 moves the original two channel bits into output ID bits 2..3.
  // Original protocol mapping: 10=ch1, 01=ch2, 11=ch3.
  const uint8_t code = static_cast<uint8_t>((id >> 2U) & 0x03U);
  if (code == 2U) return 1U;
  if (code == 1U) return 2U;
  if (code == 3U) return 3U;
  return 0U;
}

uint16_t alecto_id_for_channel(uint16_t base_id, uint8_t channel) {
  uint16_t bits = 0;
  if (channel == 1U) bits = 0x0008U;
  else if (channel == 2U) bits = 0x0004U;
  else if (channel == 3U) bits = 0x000CU;
  return static_cast<uint16_t>((base_id & static_cast<uint16_t>(~0x000CU)) | bits);
}

void add_json_alecto_metadata(std::string &json, uint8_t slot, uint8_t channel, uint16_t base_id) {
  const size_t end = json.rfind('}');
  if (end == std::string::npos) return;
  std::string extra = ",\"SLOT\":" + std::to_string(static_cast<unsigned>(slot));
  extra += ",\"CHANNEL\":" + std::to_string(static_cast<unsigned>(channel));
  extra += ",\"RFBASE\":\"" + format_hex4(base_id) + "\"";
  json.insert(end, extra);
}
}  // namespace


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
      std::string build{"v0.2.0.5 · "};
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
  ESP_LOGCONFIG(TAG, "RFLink RX compatibility bridge v0.2.0.5 (EV-first decode; 3-slot Alecto reliability gate; capability diagnostics):");
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
           "AUTO=%s; CAPTURE=%s; DECODE=%s; api=%s; net=%s; up=%lu s; heap=%u; maxblk=%u; frag=%u%%; frames=%lu; decoded=%lu; calls=%lu; skipped=%lu; decmax=%lu; cbmax=%lu; observed=%lu; framecb=%lu; irq=%lu; fast=%s; loops=%lu; ovf=%lu; rec=%lu; boosts=%lu; backlog=%lu; hist=%lu",
           this->monitoring_enabled_ ? "ON" : "OFF",
           this->receiver_->is_capture_enabled() ? "ON" : "OFF", this->decode_enabled_ ? "ON" : "OFF",
           api_ready ? "YES" : "NO", network_ready ? "ON" : "OFF",
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
           "AUTO=%s; CAPTURE=%s; DECODE=%s; api=%s; net=%s; frames=%lu; decoded=%lu; calls=%lu; skipped=%lu; decmax=%lu; ovf=%lu; rec=%lu; boosts=%lu; backlog=%lu; hist=%lu",
           this->monitoring_enabled_ ? "ON" : "OFF",
           this->receiver_->is_capture_enabled() ? "ON" : "OFF", this->decode_enabled_ ? "ON" : "OFF",
           api_ready ? "YES" : "NO", network_ready ? "ON" : "OFF",
           static_cast<unsigned long>(this->receiver_->get_frame_count()), static_cast<unsigned long>(this->message_count_),
           static_cast<unsigned long>(this->decode_calls_), static_cast<unsigned long>(this->skipped_frames_),
           static_cast<unsigned long>(this->max_decode_us_),
           static_cast<unsigned long>(this->receiver_->get_overflow_reports()),
           static_cast<unsigned long>(this->receiver_->get_recovery_count()),
           static_cast<unsigned long>(this->receiver_->get_backlog_boost_count()),
           static_cast<unsigned long>(this->receiver_->get_max_completed_backlog()),
           static_cast<unsigned long>(this->repeat_history_resets_));
#endif
  ESP_LOGI("rflink.rfdiag",
           "ALECTO soft=%lu rebuilt=%lu frames=%u strong=%u weak=%u cs=%u; EV near=%lu exact50=%lu ok=%lu last=%u",
           static_cast<unsigned long>(::rflink_legacy::get_alecto_soft_frame_count()),
           static_cast<unsigned long>(::rflink_legacy::get_alecto_reconstructed_count()),
           static_cast<unsigned>(::rflink_legacy::get_alecto_last_frames()),
           static_cast<unsigned>(::rflink_legacy::get_alecto_last_data_strong()),
           static_cast<unsigned>(::rflink_legacy::get_alecto_last_payload_weak()),
           static_cast<unsigned>(::rflink_legacy::get_alecto_last_checksum_strong()),
           static_cast<unsigned long>(::rflink_legacy::get_ev1527_near_frame_count()),
           static_cast<unsigned long>(::rflink_legacy::get_ev1527_exact50_frame_count()),
           static_cast<unsigned long>(::rflink_legacy::get_ev1527_accepted_frame_count()),
           static_cast<unsigned>(::rflink_legacy::get_ev1527_last_near_pulse_count()));
  uint8_t alecto_slot_count = 0;
  for (const auto &slot : this->alecto_slots_) if (slot.used) ++alecto_slot_count;
  ESP_LOGI("rflink.alecto.gate", "published=%lu dropped=%lu learned=%lu slots=%u",
           static_cast<unsigned long>(this->alecto_gate_published_),
           static_cast<unsigned long>(this->alecto_gate_dropped_),
           static_cast<unsigned long>(this->alecto_gate_learned_),
           static_cast<unsigned>(alecto_slot_count));
}
#endif  // USE_RFLINK_AUTO_START

void RFLinkComponent::add_configured_plugin(uint16_t plugin_id) {
  if (std::find(this->configured_plugin_ids_.begin(), this->configured_plugin_ids_.end(), plugin_id) ==
      this->configured_plugin_ids_.end())
    this->configured_plugin_ids_.push_back(plugin_id);
  this->configured_capability_mask_ |= ::rflink_legacy::plugin_capability_mask(plugin_id);
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

bool RFLinkComponent::is_diagnostic_field_configured(const char *field) const {
  const uint64_t bit = ::rflink_legacy::field_capability_mask(field);
  if (bit == 0) return false;
  return this->plugin_switch_mode_ ? (this->configured_capability_mask_ & bit) != 0
                                   : (::rflink_legacy::compiled_capability_mask() & bit) != 0;
}

bool RFLinkComponent::is_diagnostic_field_enabled(const char *field) const {
  return ::rflink_legacy::enabled_plugins_support_field(field);
}

void RFLinkComponent::register_diagnostic_(uint64_t capability, uint16_t plugin_id, uint8_t kind, void *entity) {
  if (entity == nullptr) return;
  bool configured = false;
  if (plugin_id != 0) {
    configured = !this->plugin_switch_mode_ ? this->is_plugin_compiled(plugin_id)
                                            : std::find(this->configured_plugin_ids_.begin(),
                                                        this->configured_plugin_ids_.end(), plugin_id) !=
                                                  this->configured_plugin_ids_.end();
  } else {
    configured = capability != 0 &&
                 (!this->plugin_switch_mode_ ? (::rflink_legacy::compiled_capability_mask() & capability) != 0
                                             : (this->configured_capability_mask_ & capability) != 0);
  }

  // This registration is called from the package's priority-1000 on_boot hook,
  // while ESPHome setup is still running. That is the supported window for
  // set_internal() in ESPHome 2026.9.0. Runtime plugin toggles never call it.
  if (kind == 0) static_cast<sensor::Sensor *>(entity)->set_internal(!configured);
  else if (kind == 1) static_cast<text_sensor::TextSensor *>(entity)->set_internal(!configured);
  else if (kind == 2) static_cast<binary_sensor::BinarySensor *>(entity)->set_internal(!configured);

  if (!configured) return;
  this->diagnostic_bindings_.push_back({capability, plugin_id, kind, entity});
}

void RFLinkComponent::register_diagnostic_sensor(const char *field, sensor::Sensor *entity) {
  this->register_diagnostic_(::rflink_legacy::field_capability_mask(field), 0, 0, entity);
}

void RFLinkComponent::register_diagnostic_text_sensor(const char *field, text_sensor::TextSensor *entity) {
  this->register_diagnostic_(::rflink_legacy::field_capability_mask(field), 0, 1, entity);
}

void RFLinkComponent::register_diagnostic_binary_sensor(const char *field, binary_sensor::BinarySensor *entity) {
  this->register_diagnostic_(::rflink_legacy::field_capability_mask(field), 0, 2, entity);
}

void RFLinkComponent::register_plugin_diagnostic_sensor(uint16_t plugin_id, sensor::Sensor *entity) {
  this->register_diagnostic_(0, plugin_id, 0, entity);
}

void RFLinkComponent::register_plugin_diagnostic_text_sensor(uint16_t plugin_id, text_sensor::TextSensor *entity) {
  this->register_diagnostic_(0, plugin_id, 1, entity);
}

void RFLinkComponent::register_plugin_diagnostic_binary_sensor(uint16_t plugin_id, binary_sensor::BinarySensor *entity) {
  this->register_diagnostic_(0, plugin_id, 2, entity);
}

void RFLinkComponent::refresh_diagnostic_availability_() {
  const uint64_t enabled = ::rflink_legacy::enabled_capability_mask();
  for (const auto &binding : this->diagnostic_bindings_) {
    const bool active = binding.plugin_id != 0 ? this->is_plugin_enabled(binding.plugin_id)
                                               : (enabled & binding.capability) != 0;
    if (active || binding.entity == nullptr) continue;
    if (binding.kind == 0) {
      static_cast<sensor::Sensor *>(binding.entity)->publish_state(NAN);
    } else if (binding.kind == 1) {
      auto *text = static_cast<text_sensor::TextSensor *>(binding.entity);
      if (!text->has_state() || text->state != "Kikapcsolva") text->publish_state("Kikapcsolva");
    } else if (binding.kind == 2) {
      static_cast<binary_sensor::BinarySensor *>(binding.entity)->invalidate_state();
    }
  }
}

void RFLinkComponent::publish_active_plugins() {
  if (this->active_plugins_text_sensor_ == nullptr) return;
  this->active_plugins_text_sensor_->publish_state(::rflink_legacy::enabled_plugins_csv());
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
  this->refresh_diagnostic_availability_();
  if (plugin_id == 254) {
    if (!enabled) {
      if (this->unsupported_signal_text_sensor_ != nullptr)
        this->unsupported_signal_text_sensor_->publish_state("Kikapcsolva");
      if (this->unsupported_pulse_count_sensor_ != nullptr)
        this->unsupported_pulse_count_sensor_->publish_state(NAN);
    } else if (this->unsupported_signal_text_sensor_ != nullptr) {
      this->unsupported_signal_text_sensor_->publish_state("Várakozás ismeretlen RF jelre...");
    }
  }
  return true;
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

void RFLinkPluginSwitch::dump_config() { LOG_SWITCH("  ", "RFLink plugin", this); }

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

bool RFLinkComponent::filter_alecto_message_(uint32_t now_ms) {
  std::string protocol;
  if (!json_string_field(this->message_buffer_, "NAME", protocol) || protocol != "Alecto V1") return true;

  std::string id_text;
  uint16_t full_id = 0;
  if (!json_string_field(this->message_buffer_, "ID", id_text) || !parse_hex_u16(id_text, full_id)) {
    ++this->alecto_gate_dropped_;
    return false;
  }
  // Plugin_030's displayed Alecto V1 ID has bits 0..1 clear and encodes the
  // 3-position channel selector in bits 2..3. Reject impossible IDs early.
  const uint8_t channel = alecto_channel_from_id(full_id);
  if (channel == 0U || (full_id & 0x0003U) != 0U) {
    ++this->alecto_gate_dropped_;
    return false;
  }
  const uint16_t base_id = static_cast<uint16_t>(full_id & static_cast<uint16_t>(~0x000CU));

  std::string temp_text;
  const bool has_temp = json_string_field(this->message_buffer_, "TEMP", temp_text);
  int16_t temp_tenths = 0;
  if (has_temp && !parse_rflink_temp(temp_text, temp_tenths)) {
    ++this->alecto_gate_dropped_;
    return false;
  }
  std::string battery_text;
  const bool has_battery = json_string_field(this->message_buffer_, "BAT", battery_text);
  const uint8_t battery_state = !has_battery ? 0U : (battery_text == "LOW" ? 2U : (battery_text == "OK" ? 1U : 0U));

  auto reset_samples = [](AlectoSamples &samples) { samples = AlectoSamples{}; };
  auto add_sample = [&](AlectoSamples &samples) -> bool {
    if (!has_temp) return false;
    if (samples.last_sample_ms != 0U &&
        static_cast<uint32_t>(now_ms - samples.last_sample_ms) < ALECTO_SAMPLE_MIN_SPACING_MS)
      return false;
    samples.last_sample_ms = now_ms;
    const uint8_t pos = samples.next;
    samples.temp[pos] = temp_tenths;
    samples.battery[pos] = battery_state;
    samples.next = static_cast<uint8_t>((pos + 1U) % ALECTO_SAMPLE_COUNT);
    if (samples.count < ALECTO_SAMPLE_COUNT) ++samples.count;
    return true;
  };
  auto consensus = [](const AlectoSamples &samples, int16_t &median, uint8_t &cluster_count,
                      uint8_t &battery) -> bool {
    if (samples.count < 3U) return false;
    int16_t sorted[ALECTO_SAMPLE_COUNT]{};
    for (uint8_t i = 0; i < samples.count; ++i) sorted[i] = samples.temp[i];
    std::sort(sorted, sorted + samples.count);
    uint8_t best_start = 0, best_count = 0;
    for (uint8_t start = 0; start < samples.count; ++start) {
      uint8_t count = 1;
      while (static_cast<uint8_t>(start + count) < samples.count &&
             static_cast<int32_t>(sorted[start + count]) - sorted[start] <= ALECTO_CLUSTER_SPAN_TENTHS)
        ++count;
      if (count > best_count) {
        best_start = start;
        best_count = count;
      }
    }
    if (best_count < 3U) return false;
    median = sorted[best_start + best_count / 2U];
    cluster_count = best_count;
    uint8_t ok = 0, low = 0;
    for (uint8_t i = 0; i < samples.count; ++i) {
      const int32_t delta = static_cast<int32_t>(samples.temp[i]) - median;
      if (delta < -ALECTO_CLUSTER_SPAN_TENTHS || delta > ALECTO_CLUSTER_SPAN_TENTHS) continue;
      if (samples.battery[i] == 1U) ++ok;
      else if (samples.battery[i] == 2U) ++low;
    }
    battery = low > ok ? 2U : (ok != 0U ? 1U : (low != 0U ? 2U : 0U));
    return true;
  };
  auto sanitize_and_tag = [&](AlectoSlot &slot, uint8_t slot_index, int16_t stable_temp,
                              uint8_t stable_battery) {
    replace_json_string_field(this->message_buffer_, "ID", format_hex4(slot.full_id));
    if (has_temp) replace_json_string_field(this->message_buffer_, "TEMP", format_rflink_temp(stable_temp));
    if (has_battery && stable_battery != 0U)
      replace_json_string_field(this->message_buffer_, "BAT", stable_battery == 2U ? "LOW" : "OK");
    add_json_alecto_metadata(this->message_buffer_, static_cast<uint8_t>(slot_index + 1U), slot.channel, slot.base_id);
  };

  // A known transmitter is identified by its base ID; changing the physical
  // 1/2/3 channel selector only changes the dedicated channel bits.
  int slot_index = -1;
  for (uint8_t i = 0; i < ALECTO_SLOT_COUNT; ++i) {
    if (this->alecto_slots_[i].used && this->alecto_slots_[i].base_id == base_id) {
      slot_index = i;
      break;
    }
  }

  if (slot_index >= 0) {
    auto &slot = this->alecto_slots_[slot_index];
    slot.last_seen_ms = now_ms;
    if (channel == slot.channel) {
      slot.pending_channel = 0;
      slot.pending_channel_hits = 0;
    } else if (slot.pending_channel != channel || slot.pending_channel_last_ms == 0U ||
               static_cast<uint32_t>(now_ms - slot.pending_channel_last_ms) > ALECTO_CANDIDATE_TIMEOUT_MS) {
      slot.pending_channel = channel;
      slot.pending_channel_hits = 1;
      slot.pending_channel_last_ms = now_ms;
    } else if (static_cast<uint32_t>(now_ms - slot.pending_channel_last_ms) >= ALECTO_SAMPLE_MIN_SPACING_MS) {
      slot.pending_channel_last_ms = now_ms;
      if (slot.pending_channel_hits < 255U) ++slot.pending_channel_hits;
      if (slot.pending_channel_hits >= 2U) {
        const uint8_t old_channel = slot.channel;
        slot.channel = channel;
        slot.full_id = alecto_id_for_channel(slot.base_id, slot.channel);
        slot.pending_channel = 0;
        slot.pending_channel_hits = 0;
        ESP_LOGI("rflink.alecto", "Alecto slot %u channel confirmed: %u -> %u; ID=%04X",
                 static_cast<unsigned>(slot_index + 1), static_cast<unsigned>(old_channel),
                 static_cast<unsigned>(slot.channel), static_cast<unsigned>(slot.full_id));
      }
    }

    if (!has_temp) {
      // Non-temperature Alecto packet types are allowed only for already
      // learned transmitters. They keep their raw field values but receive the
      // stable slot/channel metadata.
      sanitize_and_tag(slot, static_cast<uint8_t>(slot_index), 0, 0);
      ++this->alecto_gate_published_;
      return true;
    }

    if (!add_sample(slot.samples)) {
      ++this->alecto_gate_dropped_;
      return false;
    }
    int16_t stable_temp = 0;
    uint8_t cluster = 0, stable_battery = 0;
    if (!consensus(slot.samples, stable_temp, cluster, stable_battery)) {
      ++this->alecto_gate_dropped_;
      return false;
    }
    if (slot.last_publish_ms != 0U &&
        static_cast<uint32_t>(now_ms - slot.last_publish_ms) < ALECTO_PUBLISH_INTERVAL_MS) {
      ++this->alecto_gate_dropped_;
      return false;
    }
    if (slot.has_published_temp) {
      int32_t jump = static_cast<int32_t>(stable_temp) - slot.published_temp;
      if (jump < 0) jump = -jump;
      if (jump > ALECTO_LARGE_JUMP_TENTHS && cluster < ALECTO_SAMPLE_COUNT) {
        ++this->alecto_gate_dropped_;
        return false;
      }
    }
    sanitize_and_tag(slot, static_cast<uint8_t>(slot_index), stable_temp, stable_battery);
    slot.last_publish_ms = now_ms;
    slot.published_temp = stable_temp;
    slot.has_published_temp = true;
    reset_samples(slot.samples);
    ++this->alecto_gate_published_;
    return true;
  }

  // Unknown base ID: do not expose a single checksum-valid weak-signal packet.
  // Learn a transmitter only after >=3 time-separated temperature samples form
  // a tight cluster. This is what rejects one-off IDs such as 0044/00CC/00DC.
  if (!has_temp) {
    ++this->alecto_gate_dropped_;
    return false;
  }
  for (auto &candidate : this->alecto_candidates_) {
    if (candidate.used && static_cast<uint32_t>(now_ms - candidate.last_seen_ms) > ALECTO_CANDIDATE_TIMEOUT_MS)
      candidate = AlectoCandidate{};
  }
  AlectoCandidate *candidate = nullptr;
  for (auto &item : this->alecto_candidates_) {
    if (item.used && item.base_id == base_id) {
      candidate = &item;
      break;
    }
  }
  if (candidate == nullptr) {
    for (auto &item : this->alecto_candidates_) {
      if (!item.used) {
        candidate = &item;
        break;
      }
    }
  }
  if (candidate == nullptr) {
    candidate = &this->alecto_candidates_[0];
    for (auto &item : this->alecto_candidates_)
      if (item.last_seen_ms < candidate->last_seen_ms) candidate = &item;
  }
  if (!candidate->used || candidate->base_id != base_id) {
    *candidate = AlectoCandidate{};
    candidate->used = true;
    candidate->base_id = base_id;
    candidate->first_ms = now_ms;
  }
  candidate->last_seen_ms = now_ms;
  if (add_sample(candidate->samples) && channel <= 3U && candidate->channel_votes[channel] < 255U)
    ++candidate->channel_votes[channel];

  int16_t stable_temp = 0;
  uint8_t cluster = 0, stable_battery = 0;
  if (!consensus(candidate->samples, stable_temp, cluster, stable_battery)) {
    ++this->alecto_gate_dropped_;
    return false;
  }

  int assign = -1;
  for (uint8_t i = 0; i < ALECTO_SLOT_COUNT; ++i) {
    if (!this->alecto_slots_[i].used) {
      assign = i;
      break;
    }
  }
  if (assign < 0) {
    uint32_t stalest_age = 0;
    for (uint8_t i = 0; i < ALECTO_SLOT_COUNT; ++i) {
      const uint32_t age = static_cast<uint32_t>(now_ms - this->alecto_slots_[i].last_seen_ms);
      if (age >= ALECTO_SLOT_STALE_MS && age >= stalest_age) {
        stalest_age = age;
        assign = i;
      }
    }
  }
  if (assign < 0) {
    ++this->alecto_gate_dropped_;
    return false;
  }

  uint8_t learned_channel = channel;
  for (uint8_t ch = 1; ch <= 3; ++ch)
    if (candidate->channel_votes[ch] > candidate->channel_votes[learned_channel]) learned_channel = ch;
  auto &slot = this->alecto_slots_[assign];
  slot = AlectoSlot{};
  slot.used = true;
  slot.base_id = base_id;
  slot.channel = learned_channel;
  slot.full_id = alecto_id_for_channel(base_id, learned_channel);
  slot.last_seen_ms = now_ms;
  slot.last_publish_ms = now_ms;
  slot.has_published_temp = true;
  slot.published_temp = stable_temp;
  sanitize_and_tag(slot, static_cast<uint8_t>(assign), stable_temp, stable_battery);
  *candidate = AlectoCandidate{};
  ++this->alecto_gate_learned_;
  ++this->alecto_gate_published_;
  ESP_LOGI("rflink.alecto", "Alecto slot %u learned: base=%04X ID=%04X channel=%u temp=%.1f C",
           static_cast<unsigned>(assign + 1), static_cast<unsigned>(slot.base_id),
           static_cast<unsigned>(slot.full_id), static_cast<unsigned>(slot.channel),
           static_cast<double>(stable_temp) / 10.0);
  return true;
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

    // Plugin 254 can see dozens of noise frames per second on a 433 MHz
    // receiver. Publishing every one of them over the native API creates a
    // feedback loop (heap churn + logger/API traffic) exactly while timing
    // sensitive RF capture is running. Keep exact Alecto candidates immediate,
    // but throttle generic unsupported telemetry to 4 Hz.
    static uint32_t last_unsupported_publish_ms = 0;
    const uint32_t unsupported_now_ms = millis();
    const bool publish_unsupported = alecto_candidate || last_unsupported_publish_ms == 0 ||
        static_cast<uint32_t>(unsupported_now_ms - last_unsupported_publish_ms) >= 250U;
    if (publish_unsupported) {
      last_unsupported_publish_ms = unsupported_now_ms;
      if (this->unsupported_signal_text_sensor_ != nullptr) {
        // A 74-pulse Alecto candidate is far more useful in HA when we expose
        // the exact Plugin_030 reject reason instead of only a truncated pulse list.
        this->unsupported_signal_text_sensor_->publish_state(
            alecto_candidate ? alecto_diagnostic : unsupported.summary);
      }
      if (this->unsupported_pulse_count_sensor_ != nullptr)
        this->unsupported_pulse_count_sensor_->publish_state(unsupported.pulse_count);
    }

    if (alecto_candidate) {
      ESP_LOGW("rflink.alecto", "%s; plugin030=%s", alecto_diagnostic.c_str(),
               ::rflink_legacy::is_plugin_enabled(30) ? "ON" : "OFF");

      // Plugin 254 keeps its HA summary deliberately short. For 74-pulse
      // frames, however, the complete waveform is essential to distinguish a
      // damaged Alecto V1 frame from an unrelated 74-pulse protocol. Log the
      // full normalized waveform to serial, rate-limited to avoid making RF
      // reception worse while debugging.
      static uint32_t last_74_raw_log_ms = 0;
      const uint32_t raw_now_ms = millis();
      if (last_74_raw_log_ms == 0 ||
          static_cast<uint32_t>(raw_now_ms - last_74_raw_log_ms) >= 2000) {
        last_74_raw_log_ms = raw_now_ms;
        const auto &raw = data.get_raw_data();
        size_t first = 0;
        size_t end = raw.size();
        while (first < end && raw[first] < 0) ++first;
        if (end > first && raw[end - 1] <= -5000) --end;

        std::string part1{"74-pulse raw 1/2: "};
        std::string part2{"74-pulse raw 2/2: "};
        unsigned pulse_index = 0;
        for (size_t pos = first; pos < end; ++pos) {
          const int32_t value = raw[pos];
          const uint32_t us = static_cast<uint32_t>(value < 0 ? -static_cast<int64_t>(value) : value);
          ++pulse_index;
          std::string &dst = pulse_index <= 37 ? part1 : part2;
          if ((pulse_index != 1 && pulse_index != 38)) dst += ',';
          dst += std::to_string(us);
        }
        if (end > first && raw[end - 1] > 0) {
          ++pulse_index;
          std::string &dst = pulse_index <= 37 ? part1 : part2;
          if ((pulse_index != 1 && pulse_index != 38)) dst += ',';
          dst += "5000";
        }
        ESP_LOGW("rflink.alecto.raw", "%s", part1.c_str());
        ESP_LOGW("rflink.alecto.raw", "%s", part2.c_str());
      }
    }
    if (this->log_messages_ && publish_unsupported)
      ESP_LOGD(TAG, "Plugin 254 unsupported RF: %s%s", unsupported.summary.c_str(),
               unsupported.truncated ? " [HA summary truncated]" : "");
  }
  if (!this->message_buffer_.empty() && !this->filter_alecto_message_(now_ms))
    this->message_buffer_.clear();
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
