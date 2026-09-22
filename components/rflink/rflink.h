#pragma once
#include "rflink_gestures.h"
#include "rflink_fields.h"  // v0.1.3: data conversion and API-ready gate helpers
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace rflink {
class RFLinkComponent : public Component, public remote_base::RemoteReceiverListener {
 public:
  void setup() override;
  void dump_config() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;
  void set_log_messages(bool enabled) { this->log_messages_ = enabled; }
  // Default remains ON, preserving existing MQTT / API YAML behavior.
  // The supplied diagnostic YAML explicitly turns this OFF before the main loop.
  void set_decode_enabled(bool enabled);
  bool is_decode_enabled() const { return this->decode_enabled_; }
  uint32_t get_decode_calls() const { return this->decode_calls_; }
  uint32_t get_skipped_frames() const { return this->skipped_frames_; }
  uint32_t get_message_count() const { return this->message_count_; }
  uint32_t get_max_decode_us() const { return this->max_decode_us_; }
  uint32_t get_max_callback_us() const { return this->max_callback_us_; }
  uint32_t get_observed_frames() const { return this->observed_frames_; }
  uint32_t get_max_frame_callback_us() const { return this->max_frame_callback_us_; }
  void add_on_frame_callback(std::function<void(uint16_t, uint32_t)> &&callback) {
    this->frame_callbacks_.add(std::move(callback));
  }
  void add_on_decode_state_callback(std::function<void(bool)> &&callback) {
    this->decode_state_callbacks_.add(std::move(callback));
  }
  void add_on_message_callback(std::function<void(std::string)> &&callback) {
    this->callbacks_.add(std::move(callback));
  }
 protected:
  bool log_messages_{true};
  bool decode_enabled_{true};
  uint32_t decode_calls_{0};
  uint32_t skipped_frames_{0};
  uint32_t message_count_{0};
  uint32_t max_decode_us_{0};
  uint32_t max_callback_us_{0};
  uint32_t observed_frames_{0};
  uint32_t max_frame_callback_us_{0};
  CallbackManager<void(uint16_t, uint32_t)> frame_callbacks_;
  CallbackManager<void(bool)> decode_state_callbacks_;
  CallbackManager<void(std::string)> callbacks_;
};
class RFLinkMessageTrigger : public Trigger<std::string> {
 public:
  explicit RFLinkMessageTrigger(RFLinkComponent *parent) {
    parent->add_on_message_callback([this](std::string message) { this->trigger(std::move(message)); });
  }
};
}  // namespace rflink
}  // namespace esphome
