#pragma once
// v0.1.6: presentation/routing only. No RFLink plugin, decoder or receiver edits.
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/rflink/rflink.h"
#include "esphome/components/event/event.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome { namespace rflink_remote {
using Timing = rflink_gestures::Timing;
using Gesture = rflink_gestures::Event;

struct Match {
  std::string protocol, rf_id, button, command;
  bool operator==(const Match &other) const {
    return protocol == other.protocol && rf_id == other.rf_id &&
           button == other.button && command == other.command;
  }
  static Match ev1527(uint32_t code);
  bool valid() const;
  // A compact, self-contained match. Empty result means too long for a HA state.
  std::string json(const char *mode, const char *gesture = nullptr, uint32_t seq = 0) const;
};

class RFRemoteEvent : public event::Event {
 public:
  void set_pattern(const std::string &protocol, const std::string &rf_id,
                   const std::string &button, const std::string &command, bool gestures) {
    this->match_ = {protocol, rf_id, button, command}; this->gestures_ = gestures;
  }
  void set_timing_values(uint32_t release, uint32_t held_release, uint32_t fresh,
                         uint32_t multi, uint32_t hold, uint32_t repeat, uint32_t maximum);
  void set_event_mask(uint32_t mask) { this->event_mask_ = mask; }
  void set_message_cooldown(uint32_t ms) { this->message_cooldown_ = ms; }
  void set_log_events(bool value) { this->log_events_ = value; }
  void set_pressed_sensor(binary_sensor::BinarySensor *sensor) { this->pressed_sensor_ = sensor; }
  void initialize(const Timing &fallback);
  void observe_frame(uint16_t plugin, uint32_t code, uint32_t now);
  void observe_message(const Match &key, uint32_t now);
  void tick(uint32_t now);
  void cancel(uint32_t now);
  bool gestures() const { return this->gestures_; }
  bool valid() const { return this->valid_; }
 protected:
  void handle_gesture_(const Gesture &event);
  void emit_(const char *type);
  Match match_;
  Timing timing_{};
  rflink_gestures::Button state_;
  binary_sensor::BinarySensor *pressed_sensor_{nullptr};
  uint32_t event_mask_{0}, message_cooldown_{0}, last_message_at_{0};
  bool gestures_{true}, custom_timing_{false}, log_events_{true}, valid_{false}, have_message_{false};
};

class RFRemoteHub : public Component {
 public:
  void set_parent(rflink::RFLinkComponent *parent) { this->parent_ = parent; }
  void add_remote(RFRemoteEvent *remote) { this->remotes_.push_back(remote); }
  void set_timing_values(uint32_t release, uint32_t held_release, uint32_t fresh,
                         uint32_t multi, uint32_t hold, uint32_t repeat, uint32_t maximum);
  void configure_learning(bool enabled, uint32_t duration, uint8_t max_signals,
                           uint8_t min_frames, bool logs);
  void set_learning_signal_sensor(text_sensor::TextSensor *sensor) { this->signal_sensor_ = sensor; }
  void set_learning_gesture_sensor(text_sensor::TextSensor *sensor) { this->gesture_sensor_ = sensor; }
  void setup() override;
  void dump_config() override;
  void loop() override;
  void set_learning_enabled(bool enabled);
  bool is_learning_enabled() const { return this->learning_enabled_; }
  uint32_t get_learning_dropped_keys() const { return this->dropped_keys_; }
  // Public for deterministic tests; normal input is the existing bridge callbacks.
  void observe_frame(uint16_t plugin, uint32_t code, uint32_t now);
  void observe_message(const std::string &json, uint32_t now);
  void tick(uint32_t now);
 protected:
  struct LearningSlot {
    rflink_gestures::Button button;
    Match key;
    uint32_t code{0}, last_seen{0};
    bool used{false}, confirmed{false};
  };
  void cancel_all_(uint32_t now);
  LearningSlot *learning_slot_(uint32_t code, uint32_t now);
  void learning_gesture_(LearningSlot &slot, const Gesture &event);
  void publish_signal_(const Match &key, const char *mode);
  void publish_gesture_(const Match &key, const char *type);
  rflink::RFLinkComponent *parent_{nullptr};
  std::vector<RFRemoteEvent *> remotes_;
  // Allocated ONCE at setup; never grows in response to RF traffic.
  std::vector<LearningSlot> learning_slots_;
  Timing timing_{};
  text_sensor::TextSensor *signal_sensor_{nullptr}, *gesture_sensor_{nullptr};
  std::string last_signal_;
  uint32_t learning_duration_{60000}, learning_started_{0}, sequence_{0};
  uint32_t last_tick_{0}, dropped_keys_{0}, last_drop_log_{0};
  uint8_t max_signals_{0}, min_frames_{3};
  bool learning_enabled_{false}, learning_logs_{true}, has_message_remotes_{false};
};

} }  // namespace esphome::rflink_remote
