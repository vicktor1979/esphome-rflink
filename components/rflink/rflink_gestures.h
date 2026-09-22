#pragma once
// RFLink v0.1.4: non-blocking gesture inference for a configured EV1527 code.
// No plugin edits, raw waveform logging, JSON parsing or network access here.
// The transmitter must repeat a recognisable frame while held. No release bit
// exists in the uploaded Plugin_061 output; release is inferred from RF silence.
#include <cstdint>
#include <functional>
#include <utility>

namespace esphome {
namespace rflink_gestures {

struct Timing {
  uint32_t release_ms{180};          // Silence since the last matching RF frame.
  uint32_t multi_click_ms{350};      // Additional wait after the inferred release.
  uint32_t hold_ms{700};             // Span between REAL matching frames.
  uint32_t repeat_ms{250};           // Generated hold_repeat event cadence.
  uint32_t max_press_ms{30000};      // Cancel and require silence after a stuck key.
  bool valid() const {
    return release_ms >= 20 && release_ms <= 2000 &&
           multi_click_ms >= 50 && multi_click_ms <= 3000 &&
           hold_ms > release_ms && hold_ms <= 10000 &&
           repeat_ms >= 100 && repeat_ms <= 5000 &&
           max_press_ms > hold_ms && max_press_ms <= 300000;
  }
};

struct Event {
  const char *type;
  bool pressed;
  uint16_t clicks;
  uint32_t frames;
  uint32_t span_ms;
  uint32_t max_gap_ms;
};

class Button {
 public:
  using Callback = std::function<void(const Event &)>;
  static constexpr uint16_t MAX_CLICKS = 10;

  // RFLink ID text is HEX, including digit-only strings with leading zeros.
  bool configure_ev1527(const char *device_id, const char *button, const Timing &timing) {
    uint32_t device = 0, nibble = 0;
    this->reset_();
    this->configured_ = timing.valid() && parse_hex_(device_id, 6, 0xFFFFF, device) &&
                        parse_hex_(button, 2, 0xF, nibble);
    if (this->configured_) {
      this->code_ = (device << 4) | nibble;
      this->timing_ = timing;
    }
    return this->configured_;
  }
  void set_callback(Callback callback) { this->callback_ = std::move(callback); }
  bool configured() const { return this->configured_; }
  bool pressed() const { return this->pressed_; }
  bool holding() const { return this->held_; }
  uint32_t matching_frames() const { return this->total_frames_; }
  uint32_t code() const { return this->code_; }

  bool observe(uint16_t plugin_id, uint32_t code, uint32_t now) {
    if (!this->configured_ || plugin_id != 61 || code != this->code_) return false;
    // Evaluate an RF gap before overwriting last_seen_, even after a delayed loop.
    this->tick(now);
    ++this->total_frames_;
    if (this->blocked_) {
      this->last_seen_ = now;  // Continuously stuck transmitters cannot rearm.
      return true;
    }
    if (!this->pressed_) {
      this->pressed_ = true;
      this->held_ = false;
      this->first_seen_ = this->last_seen_ = now;
      this->stroke_frames_ = 1;
      this->max_gap_ = 0;
      this->emit_("press", now, this->clicks_);
      return true;
    }
    const uint32_t gap = elapsed_(now, this->last_seen_);
    if (gap > this->max_gap_) this->max_gap_ = gap;
    this->last_seen_ = now;
    if (this->stroke_frames_ != UINT32_MAX) ++this->stroke_frames_;
    // A timer alone MUST NOT turn one isolated packet into a held key.
    // Require at least 3 actual frames, spanning the configured hold threshold.
    if (!this->held_ && this->stroke_frames_ >= 3 &&
        elapsed_(now, this->first_seen_) >= this->timing_.hold_ms) {
      this->held_ = true;
      this->clicks_ = 0;  // A short-click-then-hold sequence does NOT emit a click.
      this->last_repeat_ = now;
      this->emit_("hold", now, 0);
    }
    return true;
  }

  void tick(uint32_t now) {
    if (!this->configured_) return;
    if (this->blocked_) {
      if (elapsed_(now, this->last_seen_) >= this->timing_.release_ms)
        this->blocked_ = false;
      return;
    }
    if (this->pressed_) {
      // Silence takes priority. A late loop must not synthesize hold repeats.
      if (elapsed_(now, this->last_seen_) >= this->timing_.release_ms) {
        this->end_stroke_(now);
      } else if (elapsed_(now, this->first_seen_) >= this->timing_.max_press_ms) {
        this->pressed_ = this->held_ = false;
        this->clicks_ = 0;
        this->blocked_ = true;
        this->emit_("cancel", now, 0);
        return;
      } else if (this->held_ && elapsed_(now, this->last_repeat_) >= this->timing_.repeat_ms) {
        this->last_repeat_ = now;
        this->emit_("hold_repeat", now, 0);  // At most one, never catch-up bursts.
      }
    }
    if (!this->pressed_ && this->clicks_ > 0 &&
        elapsed_(now, this->released_at_) >= this->timing_.multi_click_ms) {
      const uint16_t count = this->clicks_;
      this->clicks_ = 0;
      this->emit_(click_type_(count), now, count);
    }
  }

  // Decoder/API pause cancels active AND pending clicks; it does not confirm them.
  void cancel(uint32_t now) {
    const bool notify = this->pressed_ || this->clicks_ != 0 || this->blocked_;
    this->pressed_ = this->held_ = this->blocked_ = false;
    this->clicks_ = 0;
    if (notify) this->emit_("cancel", now, 0);
  }

 protected:
  static uint32_t elapsed_(uint32_t now, uint32_t before) { return now - before; }
  static bool parse_hex_(const char *s, uint8_t max_chars, uint32_t max_value, uint32_t &value) {
    if (s == nullptr || *s == '\0') return false;
    value = 0;
    uint8_t length = 0;
    for (; *s; ++s) {
      if (++length > max_chars) return false;
      uint8_t digit;
      if (*s >= '0' && *s <= '9') digit = *s - '0';
      else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
      else if (*s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
      else return false;
      value = (value << 4) | digit;
      if (value > max_value) return false;
    }
    return true;
  }
  static const char *click_type_(uint16_t count) {
    switch (count) {
      case 1: return "single";
      case 2: return "double";
      case 3: return "triple";
      case 4: return "click_4";
      case 5: return "click_5";
      case 6: return "click_6";
      case 7: return "click_7";
      case 8: return "click_8";
      case 9: return "click_9";
      case 10: return "click_10";
      default: return "multi_overflow";
    }
  }
  void end_stroke_(uint32_t now) {
    const bool was_held = this->held_;
    this->pressed_ = this->held_ = false;
    this->released_at_ = this->last_seen_ + this->timing_.release_ms;
    if (!was_held && this->clicks_ <= MAX_CLICKS) ++this->clicks_;
    this->emit_("release", now, this->clicks_);
    if (was_held) this->emit_("hold_release", now, 0);
  }
  void emit_(const char *type, uint32_t /*now*/, uint16_t clicks) {
    if (this->callback_)
      this->callback_(Event{type, this->pressed_, clicks, this->stroke_frames_,
                            elapsed_(this->last_seen_, this->first_seen_), this->max_gap_});
  }
  void reset_() {
    this->pressed_ = this->held_ = this->blocked_ = false;
    this->clicks_ = 0;
    this->first_seen_ = this->last_seen_ = this->last_repeat_ = this->released_at_ = 0;
    this->stroke_frames_ = this->max_gap_ = this->total_frames_ = 0;
  }
  Timing timing_{};
  Callback callback_;
  uint32_t code_{0};
  uint32_t first_seen_{0}, last_seen_{0}, last_repeat_{0}, released_at_{0};
  uint32_t stroke_frames_{0}, max_gap_{0}, total_frames_{0};
  uint16_t clicks_{0};
  bool configured_{false}, pressed_{false}, held_{false}, blocked_{false};
};

}  // namespace rflink_gestures
}  // namespace esphome
