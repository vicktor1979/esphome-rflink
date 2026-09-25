#pragma once
#include "rflink_gestures.h"
#include "rflink_fields.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace binary_sensor {
class BinarySensor;
}
namespace remote_receiver {
class RemoteReceiverComponent;
}
namespace rflink {

class RFLinkComponent : public Component, public remote_base::RemoteReceiverListener {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;
  void set_log_messages(bool enabled) { this->log_messages_ = enabled; }
  void set_plugin_switch_mode(bool enabled) { this->plugin_switch_mode_ = enabled; }
  void set_decode_enabled(bool enabled);
  bool is_decode_enabled() const { return this->decode_enabled_; }
  uint32_t get_decode_calls() const { return this->decode_calls_; }
  uint32_t get_skipped_frames() const { return this->skipped_frames_; }
  uint32_t get_message_count() const { return this->message_count_; }
  uint32_t get_max_decode_us() const { return this->max_decode_us_; }
  uint32_t get_max_callback_us() const { return this->max_callback_us_; }
  uint32_t get_observed_frames() const { return this->observed_frames_; }
  uint32_t get_max_frame_callback_us() const { return this->max_frame_callback_us_; }
  uint32_t get_repeat_history_resets() const { return this->repeat_history_resets_; }

  // Optional v0.1.9 built-in startup/diagnostics controller. It is compiled in
  // only when auto_start is configured, preserving old external-component use.
  void set_receiver(remote_receiver::RemoteReceiverComponent *receiver) { this->receiver_ = receiver; }
  void set_auto_start_enabled(bool enabled) { this->auto_start_enabled_ = enabled; }
  void set_auto_start_settle_ms(uint32_t value) { this->auto_start_settle_ms_ = value; }
  void set_diagnostics_interval_ms(uint32_t value) { this->diagnostics_interval_ms_ = value; }
  void set_require_network(bool value) { this->require_network_ = value; }
  void set_require_api(bool value) { this->require_api_ = value; }
  void set_monitoring_enabled(bool enabled);
  bool is_monitoring_enabled() const { return this->monitoring_enabled_; }
  void set_ota_active(bool active);
  bool is_ota_active() const { return this->ota_active_; }
  void set_decode_active_sensor(binary_sensor::BinarySensor *sensor) { this->decode_active_sensor_ = sensor; }
  void set_health_text_sensor(text_sensor::TextSensor *sensor) { this->health_text_sensor_ = sensor; }
  void set_build_text_sensor(text_sensor::TextSensor *sensor) { this->build_text_sensor_ = sensor; }

  bool set_plugin_enabled(uint16_t plugin_id, bool enabled);
  bool is_plugin_enabled(uint16_t plugin_id) const;
  bool is_plugin_compiled(uint16_t plugin_id) const;
  size_t get_enabled_plugin_count() const;
  void set_active_plugins_text_sensor(text_sensor::TextSensor *sensor) { this->active_plugins_text_sensor_ = sensor; }
  void set_unsupported_signal_text_sensor(text_sensor::TextSensor *sensor) { this->unsupported_signal_text_sensor_ = sensor; }
  void set_unsupported_pulse_count_sensor(sensor::Sensor *sensor) { this->unsupported_pulse_count_sensor_ = sensor; }
  void publish_active_plugins();

  void add_on_frame_callback(std::function<void(uint16_t, uint32_t)> &&callback) {
    this->frame_callbacks_.add(std::move(callback));
  }
  void add_on_decode_state_callback(std::function<void(bool)> &&callback) {
    this->decode_state_callbacks_.add(std::move(callback));
  }
  void add_on_message_observer(std::function<void(const std::string &)> &&callback) {
    this->message_observers_.add(std::move(callback));
  }
  void add_on_message_callback(std::function<void(std::string)> &&callback) {
    this->callbacks_.add(std::move(callback));
  }

 protected:
  void reset_runtime_history_(const char *reason);
#ifdef USE_RFLINK_AUTO_START
  bool network_ready_() const;
  bool api_ready_() const;
  void apply_auto_state_(bool enabled);
  void publish_health_(const char *state);
  void update_health_(bool network_ready, bool api_ready);
  void update_diagnostics_(uint32_t now, bool network_ready, bool api_ready);
#endif

  bool log_messages_{true};
  bool plugin_switch_mode_{false};
  bool decode_enabled_{true};
  uint32_t decode_calls_{0};
  uint32_t skipped_frames_{0};
  uint32_t message_count_{0};
  uint32_t max_decode_us_{0};
  uint32_t max_callback_us_{0};
  uint32_t observed_frames_{0};
  uint32_t max_frame_callback_us_{0};
  uint32_t last_recognized_ms_{0};
  uint32_t repeat_history_resets_{0};
  bool repeat_history_dirty_{false};

  remote_receiver::RemoteReceiverComponent *receiver_{nullptr};
  binary_sensor::BinarySensor *decode_active_sensor_{nullptr};
  text_sensor::TextSensor *health_text_sensor_{nullptr};
  text_sensor::TextSensor *build_text_sensor_{nullptr};
  bool auto_start_enabled_{false};
  bool monitoring_enabled_{true};
  bool ota_active_{false};
  bool require_network_{true};
  bool require_api_{true};
  bool auto_running_{false};
  bool ready_timing_{false};
  uint32_t ready_since_ms_{0};
  uint32_t auto_start_settle_ms_{5000};
  uint32_t diagnostics_interval_ms_{30000};
  uint32_t last_auto_check_ms_{0};
  uint32_t last_diagnostics_ms_{0};
  uint32_t last_receiver_recovery_count_{0};
  std::string last_health_;

  text_sensor::TextSensor *active_plugins_text_sensor_{nullptr};
  text_sensor::TextSensor *unsupported_signal_text_sensor_{nullptr};
  sensor::Sensor *unsupported_pulse_count_sensor_{nullptr};
  CallbackManager<void(uint16_t, uint32_t)> frame_callbacks_;
  CallbackManager<void(bool)> decode_state_callbacks_;
  CallbackManager<void(const std::string &)> message_observers_;
  CallbackManager<void(std::string)> callbacks_;
  // Reused between receive calls to avoid heap churn on every decoded packet.
  std::string message_buffer_;
};

class RFLinkPluginSwitch : public switch_::Switch, public Component {
 public:
  RFLinkPluginSwitch(RFLinkComponent *parent, uint16_t plugin_id) : parent_(parent), plugin_id_(plugin_id) {}
  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
  RFLinkComponent *parent_;
  uint16_t plugin_id_;
};

class RFLinkMonitoringSwitch : public switch_::Switch, public Component {
 public:
  explicit RFLinkMonitoringSwitch(RFLinkComponent *parent) : parent_(parent) {}
  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
  RFLinkComponent *parent_;
};

class RFLinkMessageTrigger : public Trigger<std::string> {
 public:
  explicit RFLinkMessageTrigger(RFLinkComponent *parent) {
    parent->add_on_message_callback([this](std::string message) { this->trigger(std::move(message)); });
  }
};
}  // namespace rflink
}  // namespace esphome
