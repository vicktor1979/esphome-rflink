#include "rflink_remote.h"
#include <cstdio>
#include <cstring>
#include <utility>
#include "esphome/components/json/json_util.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome { namespace rflink_remote {
static const char *const TAG = "rflink.remote";
static const char *const LEARN_TAG = "rflink.learn";
static const char *const TYPES[] = {
  "press", "release", "single", "double", "triple", "click_4", "click_5", "click_6",
  "click_7", "click_8", "click_9", "click_10", "hold", "hold_repeat", "hold_release",
  "cancel", "multi_overflow", "received"
};
static uint32_t event_bit(const char *type) {
  for (size_t i = 0; i < sizeof(TYPES) / sizeof(TYPES[0]); ++i)
    if (std::strcmp(TYPES[i], type) == 0) return uint32_t{1} << i;
  return 0;
}
static uint32_t multi_click_event_mask() {
  return event_bit("double") | event_bit("triple") | event_bit("click_4") |
         event_bit("click_5") | event_bit("click_6") | event_bit("click_7") |
         event_bit("click_8") | event_bit("click_9") | event_bit("click_10") |
         event_bit("multi_overflow");
}
static Timing make_timing(uint32_t release, uint32_t held_release, uint32_t fresh,
                          uint32_t multi, uint32_t hold, uint32_t repeat, uint32_t maximum) {
  Timing t;
  t.release_ms = release; t.hold_release_ms = held_release; t.repeat_fresh_ms = fresh;
  t.multi_click_ms = multi; t.hold_ms = hold; t.repeat_ms = repeat; t.max_press_ms = maximum;
  return t;
}
static bool text_valid(const std::string &s, size_t maximum, bool empty_ok) {
  if ((!empty_ok && s.empty()) || s.size() > maximum) return false;
  for (unsigned char c : s) if (c < 32 || c == 127) return false;
  return true;
}
static void quoted(std::string &out, const std::string &s) {
  out += '"';
  for (char c : s) { if (c == '"' || c == '\\') out += '\\'; out += c; }
  out += '"';
}
Match Match::ev1527(uint32_t code) {
  char id[7], button[3];
  std::snprintf(id, sizeof(id), "%06lx", static_cast<unsigned long>((code >> 4) & 0xFFFFF));
  std::snprintf(button, sizeof(button), "%02x", static_cast<unsigned>(code & 15));
  return {"EV1527", id, button, "ON"};
}
bool Match::valid() const {
  return text_valid(this->protocol, 32, false) && text_valid(this->rf_id, 32, false) &&
         text_valid(this->button, 16, true) && text_valid(this->command, 32, true);
}
std::string Match::json(const char *mode, const char *gesture, uint32_t seq) const {
  if (!this->valid()) return {};
  std::string out; out.reserve(240);
  out = "{\"protocol\":"; quoted(out, this->protocol);
  out += ",\"rf_id\":"; quoted(out, this->rf_id);
  out += ",\"button\":"; quoted(out, this->button);
  out += ",\"command\":"; quoted(out, this->command);
  out += ",\"mode\":"; quoted(out, mode);
  if (gesture != nullptr) {
    out += ",\"gesture\":"; quoted(out, gesture);
    out += ",\"seq\":"; out += std::to_string(seq);
  }
  out += '}';
  // Never truncate identifiers: truncated keys could bind the wrong remote.
  if (out.size() > 250) return {};
  return out;
}
std::string Match::compact(const char *mode, const char *gesture) const {
  if (!this->valid()) return {};
  std::string out;
  out.reserve(96);
  out += this->protocol;
  out += " · "; out += this->rf_id;
  if (!this->button.empty()) { out += " · "; out += this->button; }
  if (!this->command.empty()) { out += " · "; out += this->command; }
  if (gesture != nullptr && *gesture != '\0') { out += " · "; out += gesture; }
  else if (mode != nullptr && *mode != '\0') { out += " · "; out += mode; }
  return out.size() <= 160 ? out : std::string{};
}
void RFRemoteEvent::set_timing_values(uint32_t r, uint32_t hr, uint32_t f,
                                     uint32_t m, uint32_t h, uint32_t p, uint32_t maximum) {
  this->timing_ = make_timing(r, hr, f, m, h, p, maximum); this->custom_timing_ = true;
}
void RFRemoteEvent::initialize(const Timing &fallback) {
  if (!this->custom_timing_) this->timing_ = fallback;
  this->valid_ = this->match_.valid();
  if (this->gestures_) {
    this->valid_ = this->valid_ && this->match_.protocol == "EV1527" && this->match_.command == "ON" &&
      this->state_.configure_ev1527(this->match_.rf_id.c_str(), this->match_.button.c_str(), this->timing_);
    this->state_.set_immediate_single((this->event_mask_ & multi_click_event_mask()) == 0);
    this->state_.set_callback([this](const Gesture &e) { this->handle_gesture_(e); });
    if (this->pressed_sensor_ != nullptr) this->pressed_sensor_->publish_state(false);
  }
  if (!this->valid_) ESP_LOGE(TAG, "Invalid remote match; this event is disabled");
  else ESP_LOGCONFIG(TAG, "Binding: %s/%s/%s/%s; mode=%s", this->match_.protocol.c_str(),
      this->match_.rf_id.c_str(), this->match_.button.c_str(), this->match_.command.c_str(),
      this->gestures_ ? "gestures" : "message");
}
void RFRemoteEvent::emit_(const char *type) {
  if ((this->event_mask_ & event_bit(type)) != 0) this->trigger(type);
}
void RFRemoteEvent::handle_gesture_(const Gesture &e) {
  if (this->pressed_sensor_ != nullptr &&
      (!this->pressed_sensor_->has_state() || this->pressed_sensor_->state != e.pressed))
    this->pressed_sensor_->publish_state(e.pressed);
  this->emit_(e.type);
  if (this->log_events_)
    ESP_LOGD(TAG, "%s/%s/%s -> %s; frames=%lu; span_ms=%lu; silence_ms=%lu; bridged=%lu",
        this->match_.protocol.c_str(), this->match_.rf_id.c_str(), this->match_.button.c_str(), e.type,
        static_cast<unsigned long>(e.frames), static_cast<unsigned long>(e.span_ms),
        static_cast<unsigned long>(e.silence_ms), static_cast<unsigned long>(e.bridged_gaps));
}
void RFRemoteEvent::observe_frame(uint16_t plugin, uint32_t code, uint32_t now) {
  if (this->valid_ && this->gestures_) this->state_.observe(plugin, code, now);
}
void RFRemoteEvent::observe_message(const Match &key, uint32_t now) {
  if (!this->valid_ || this->gestures_ || !(key == this->match_)) return;
  if (this->have_message_ && static_cast<uint32_t>(now - this->last_message_at_) < this->message_cooldown_) return;
  this->have_message_ = true; this->last_message_at_ = now;
  this->emit_("received");
  if (this->log_events_) ESP_LOGD(TAG, "%s/%s/%s/%s -> received", key.protocol.c_str(),
      key.rf_id.c_str(), key.button.c_str(), key.command.c_str());
}
void RFRemoteEvent::tick(uint32_t now) { if (this->valid_ && this->gestures_) this->state_.tick(now); }
void RFRemoteEvent::cancel(uint32_t now) {
  if (this->valid_ && this->gestures_) this->state_.cancel(now);
  this->have_message_ = false;
}
void RFRemoteHub::set_timing_values(uint32_t r, uint32_t hr, uint32_t f,
                                   uint32_t m, uint32_t h, uint32_t p, uint32_t maximum) {
  this->timing_ = make_timing(r, hr, f, m, h, p, maximum);
}
void RFRemoteHub::configure_learning(bool enabled, uint32_t duration, uint8_t maximum,
                                     uint8_t min_frames, bool logs) {
  this->learning_enabled_ = enabled; this->learning_duration_ = duration;
  this->max_signals_ = maximum <= 8 ? maximum : 8;
  this->min_frames_ = min_frames; this->learning_logs_ = logs;
}
void RFRemoteHub::setup() {
  if (this->parent_ == nullptr || !this->timing_.valid()) {
    ESP_LOGE(TAG, "Missing RFLink parent or invalid timing"); this->mark_failed(); return;
  }
  this->gesture_remotes_.clear();
  this->message_remotes_.clear();
  this->gesture_remotes_.reserve(this->remotes_.size());
  this->message_remotes_.reserve(this->remotes_.size());
  for (auto *entry : this->remotes_) {
    entry->initialize(this->timing_);
    if (!entry->valid()) continue;
    if (entry->gestures()) this->gesture_remotes_.push_back(entry);
    else this->message_remotes_.push_back(entry);
  }
  this->has_message_remotes_ = !this->message_remotes_.empty();
  this->learning_slots_.resize(this->max_signals_);
  for (auto &slot : this->learning_slots_) {
    LearningSlot *p = &slot;
    slot.button.set_callback([this, p](const Gesture &e) { this->learning_gesture_(*p, e); });
  }
  this->learning_started_ = millis();
  this->parent_->add_on_frame_callback([this](uint16_t plugin, uint32_t code) {
    this->observe_frame(plugin, code, millis());
  });
  this->parent_->add_on_message_observer([this](const std::string &message) {
    this->observe_message(message, millis());
  });
  this->parent_->add_on_decode_state_callback([this](bool enabled) {
    if (!enabled) this->cancel_all_(millis());
  });
}
void RFRemoteHub::dump_config() {
  ESP_LOGCONFIG(TAG, "v0.1.9: YAML-configured remotes; fast single-click; compact learning; slots=%u",
                static_cast<unsigned>(this->learning_slots_.size()));
  ESP_LOGCONFIG(TAG, "Configured event entities: %u; learning: %s (timeout %lu ms)",
                static_cast<unsigned>(this->remotes_.size()), this->learning_enabled_ ? "ON" : "OFF",
                static_cast<unsigned long>(this->learning_duration_));
}
void RFRemoteHub::loop() {
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - this->last_tick_) < 20) return;
  this->last_tick_ = now; this->tick(now);
}
void RFRemoteHub::tick(uint32_t now) {
  if (this->learning_enabled_ && static_cast<uint32_t>(now - this->learning_started_) >= this->learning_duration_)
    this->set_learning_enabled(false);
  if (this->parent_ == nullptr || !this->parent_->is_decode_enabled()) { this->cancel_all_(now); return; }
  for (auto *entry : this->gesture_remotes_) entry->tick(now);
  if (this->learning_enabled_)
    for (auto &slot : this->learning_slots_) if (slot.used) slot.button.tick(now);
}
void RFRemoteHub::cancel_all_(uint32_t now) {
  for (auto *entry : this->gesture_remotes_) entry->cancel(now);
  for (auto *entry : this->message_remotes_) entry->cancel(now);
  for (auto &slot : this->learning_slots_) {
    if (slot.used) slot.button.cancel(now);
    slot.used = slot.confirmed = false;
  }
}
void RFRemoteHub::set_learning_enabled(bool enabled) {
  if (enabled && this->max_signals_ == 0) {
    ESP_LOGW(LEARN_TAG, "No learning section in YAML"); return;
  }
  const uint32_t now = millis();
  // Enable again deliberately renews a session and its timeout.
  this->learning_enabled_ = enabled; this->learning_started_ = now;
  for (auto &slot : this->learning_slots_) {
    // Do not publish a synthetic release/click when clearing learning state.
    slot.used = slot.confirmed = false; slot.button.cancel(now);
  }
  this->last_signal_.clear();
  ESP_LOGI(LEARN_TAG, "Learning %s; configured remote entities are unchanged", enabled ? "ON" : "OFF");
}
RFRemoteHub::LearningSlot *RFRemoteHub::learning_slot_(uint32_t code, uint32_t now) {
  LearningSlot *candidate = nullptr;
  uint32_t oldest_age = 0;
  for (auto &slot : this->learning_slots_) {
    // RX callbacks can precede the next scheduled loop; expire stale presses
    // before declaring a slot busy, without resetting a pending real gesture.
    if (slot.used) slot.button.tick(now);
    if (slot.used && slot.code == code) return &slot;
    if (!slot.used) { if (candidate == nullptr || candidate->used) candidate = &slot; continue; }
    // Protect held keys AND pending multi-click decisions. A continuously stuck
    // key is not evicted/rearmed: its last_seen keeps updating after cancel.
    const uint32_t age = now - slot.last_seen;
    if (!slot.button.pressed() && age > this->timing_.held_release_timeout() + this->timing_.multi_click_ms &&
        (candidate == nullptr || (candidate->used && age > oldest_age))) {
      candidate = &slot; oldest_age = age;
    }
  }
  if (candidate == nullptr) {
    ++this->dropped_keys_;
    if (static_cast<uint32_t>(now - this->last_drop_log_) >= 5000) {
      this->last_drop_log_ = now;
      ESP_LOGW(LEARN_TAG, "All learning slots busy; unknown key skipped (configured remotes unaffected)");
    }
    return nullptr;
  }
  candidate->used = false; candidate->confirmed = false;
  candidate->button.cancel(now);
  candidate->key = Match::ev1527(code); candidate->code = code; candidate->last_seen = now;
  if (!candidate->button.configure_ev1527(candidate->key.rf_id.c_str(), candidate->key.button.c_str(), this->timing_))
    return nullptr;
  candidate->used = true;
  return candidate;
}
void RFRemoteHub::observe_frame(uint16_t plugin, uint32_t code, uint32_t now) {
  if (this->parent_ == nullptr || !this->parent_->is_decode_enabled()) return;
  for (auto *entry : this->gesture_remotes_) entry->observe_frame(plugin, code, now);
  if (!this->learning_enabled_ || plugin != 61 || code > 0xFFFFFF) return;
  LearningSlot *slot = this->learning_slot_(code, now);
  if (slot != nullptr) { slot->button.observe(plugin, code, now); slot->last_seen = now; }
}
void RFRemoteHub::learning_gesture_(LearningSlot &slot, const Gesture &e) {
  if (!this->learning_enabled_ || !slot.used) return;
  if (!slot.confirmed && e.frames >= this->min_frames_) slot.confirmed = true;
  if (!slot.confirmed) return;
  // Suppressed repeats still drive the Button. Publish only gestures, not every
  // RF frame; raw press/release are omitted from this discovery-only channel.
  if (std::strcmp(e.type, "press") == 0 || std::strcmp(e.type, "release") == 0) {
    if (std::strcmp(e.type, "release") == 0) this->publish_signal_(slot.key, "gestures");
    return;
  }
  this->publish_signal_(slot.key, "gestures");
  this->publish_gesture_(slot.key, e.type);
}
void RFRemoteHub::publish_signal_(const Match &key, const char *mode) {
  const std::string machine = key.json(mode);
  const std::string value = key.compact(mode);
  if (machine.empty() || value.empty()) {
    ESP_LOGW(LEARN_TAG, "Match too long for learning sensor; use RF message log");
    return;
  }
  if (machine == this->last_signal_) return;
  this->last_signal_ = machine;
  if (this->signal_sensor_ != nullptr) this->signal_sensor_->publish_state(value);
  // Keep the exact machine-readable form in the log for copy/paste into YAML.
  if (this->learning_logs_) ESP_LOGI(LEARN_TAG, "YAML match: %s", machine.c_str());
}
void RFRemoteHub::publish_gesture_(const Match &key, const char *type) {
  const uint32_t sequence = ++this->sequence_;
  const std::string machine = key.json("gestures", type, sequence);
  const std::string value = key.compact("gestures", type);
  if (machine.empty() || value.empty()) return;
  if (this->gesture_sensor_ != nullptr) this->gesture_sensor_->publish_state(value);
  if (this->learning_logs_) ESP_LOGD(LEARN_TAG, "%s", machine.c_str());
}
void RFRemoteHub::observe_message(const std::string &message, uint32_t now) {
  if (this->parent_ == nullptr || !this->parent_->is_decode_enabled() ||
      (!this->learning_enabled_ && !this->has_message_remotes_) || message.size() > 2048) return;
  json::parse_json(message, [&](JsonObject root) -> bool {
    // These RFLink identity fields are strings. Do not coerce numeric values:
    // leading zeros and protocol-specific formatting must be preserved.
    if (!root["NAME"].is<const char *>() || !root["ID"].is<const char *>()) return false;
    if ((!root["SWITCH"].isNull() && !root["SWITCH"].is<const char *>()) ||
        (!root["CMD"].isNull() && !root["CMD"].is<const char *>())) return false;
    Match key{root["NAME"] | "", root["ID"] | "", root["SWITCH"] | "", root["CMD"] | ""};
    if (!key.valid()) return false;
    for (auto *entry : this->message_remotes_) entry->observe_message(key, now);
    // EV1527 learning uses verified per-frame observations, not JSON dedup.
    // Other protocols are reported as MESSAGE-only; no invented holds/clicks.
    if (this->learning_enabled_ && key.protocol != "EV1527") this->publish_signal_(key, "message");
    return true;
  });
}

} }  // namespace esphome::rflink_remote
