// Copyright (c) 2019 ESPHome. SPDX-License-Identifier: GPL-3.0-only
// Derived from ESPHome 2026.9.0 remote_receiver.cpp. Modified 2026-09-23.
// The original ESP8266 pulse collection algorithm is retained. Additions:
// pre-setup capture gate, pin-IRQ detach/reattach, ring reset, edge counter.
// rxgate2: independently configurable high-frequency-loop request.
// v0.1.9.1: adaptive, bounded scheduler boost when completed frames back up.
// The ISR, filter, timestamping and one-frame-per-loop delivery are unchanged.
#include "remote_receiver.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <new>

namespace esphome::remote_receiver {
static const char *const TAG = "remote_receiver";
static void IRAM_ATTR HOT write_value(RemoteReceiverComponentStore *arg, uint32_t delta, bool level) {
  int32_t multiplier = ((int32_t) level << 1) - 1;
  uint32_t buffer_write = arg->buffer_write;
  arg->buffer[buffer_write++] = (int32_t) delta * multiplier;
  if (buffer_write >= arg->buffer_size) buffer_write = 0;
  if (buffer_write == arg->buffer_read) {
    buffer_write = arg->buffer_start;
    arg->overflow = true;
  }
  if (delta >= arg->idle_us) {
    if (arg->buffer_write == arg->buffer_start) {
      buffer_write = arg->buffer_start;
    } else {
      arg->buffer_start = buffer_write;
    }
  }
  arg->buffer_write = buffer_write;
}
static void IRAM_ATTR HOT commit_value(RemoteReceiverComponentStore *arg, uint32_t now, bool level) {
  if (level != arg->commit_level) {
    write_value(arg, now - arg->commit_micros, level);
    arg->commit_micros = now;
    arg->commit_level = level;
  }
}
void IRAM_ATTR HOT RemoteReceiverComponentStore::gpio_intr(RemoteReceiverComponentStore *arg) {
  arg->edge_count = arg->edge_count + 1U;
  const bool curr_level = !arg->pin.digital_read();
  const uint32_t curr_micros = micros();
  const bool prev_level = arg->prev_level;
  const uint32_t prev_micros = arg->prev_micros;
  if (curr_micros - prev_micros >= arg->filter_us && prev_level != curr_level) {
    commit_value(arg, prev_micros, prev_level);
  }
  arg->prev_micros = curr_micros;
  arg->prev_level = curr_level;
}

void RemoteReceiverComponent::reset_capture_state_() {
  auto &s = this->store_;
  // Our GPIO ISR is detached. Leave the Wi-Fi/system interrupts enabled.
  s.buffer_write = 0;
  s.buffer_start = 0;
  s.buffer_read = 0;
  s.overflow = false;
  const uint32_t now = micros();
  const bool level = this->pin_->digital_read();
  s.prev_micros = now;
  s.commit_micros = now;
  s.prev_level = level;
  s.commit_level = level;
  this->temp_.clear();  // retain capacity; no allocation on every reconnection
  this->edge_activity_pending_ = false;
  this->last_edge_count_seen_ = s.edge_count;
}



uint32_t RemoteReceiverComponent::completed_backlog_entries_() const {
  const auto &s = this->store_;
  const uint32_t read = s.buffer_read;
  const uint32_t start = s.buffer_start;
  if (start >= read) return start - read;
  return s.buffer_size - read + start;
}

void RemoteReceiverComponent::stop_backlog_boost_(uint32_t now_ms) {
  if (!this->backlog_boost_active_) return;
  this->backlog_boost_active_ = false;
  this->last_backlog_boost_stop_ms_ = now_ms;
  if (!this->high_frequency_) this->high_freq_.stop();
}

void RemoteReceiverComponent::update_backlog_boost_(uint32_t now_ms) {
  if (!this->capture_active_ || this->is_failed()) return;

  const uint32_t backlog = this->completed_backlog_entries_();
  if (backlog > this->max_completed_backlog_) this->max_completed_backlog_ = backlog;

  // A permanently enabled high-frequency loop is still controlled only by the
  // explicit YAML option. Adaptive boost is for high_frequency:false only.
  if (this->high_frequency_) {
    this->backlog_boost_active_ = false;
    return;
  }

  uint32_t high_water = this->store_.buffer_size / 3U;
  if (high_water < 64U) high_water = 64U;
  if (high_water >= this->store_.buffer_size) high_water = this->store_.buffer_size - 1U;
  uint32_t low_water = high_water / 3U;
  if (low_water < 16U) low_water = 16U;
  if (low_water >= high_water) low_water = high_water / 2U;

  // Keep boosts intentionally short. The old always-fast mode could starve
  // ESP8266 Wi-Fi; 8 ms bursts with at least 20 ms between bursts let the
  // receiver catch up while retaining scheduler/Wi-Fi breathing room.
  static constexpr uint32_t BOOST_MAX_MS = 8;
  static constexpr uint32_t BOOST_COOLDOWN_MS = 20;

  if (this->backlog_boost_active_) {
    if (backlog <= low_water ||
        static_cast<uint32_t>(now_ms - this->backlog_boost_started_ms_) >= BOOST_MAX_MS) {
      this->stop_backlog_boost_(now_ms);
    }
    return;
  }

  const bool cooldown_done = this->last_backlog_boost_stop_ms_ == 0 ||
                             static_cast<uint32_t>(now_ms - this->last_backlog_boost_stop_ms_) >= BOOST_COOLDOWN_MS;
  if (backlog >= high_water && cooldown_done) {
    this->backlog_boost_active_ = true;
    this->backlog_boost_started_ms_ = now_ms;
    ++this->backlog_boost_count_;
    this->high_freq_.start();
  }
}

void RemoteReceiverComponent::recover_capture_(const char *reason, bool log_warning) {
  if (!this->capture_ready_ || !this->capture_active_ || this->is_failed()) return;
  // The ring contents are already unusable after overflow/stall. Re-arm only
  // our GPIO interrupt; do not touch Wi-Fi/system interrupts or reallocate RAM.
  this->stop_backlog_boost_(millis());
  this->pin_->detach_interrupt();
  this->reset_capture_state_();
  this->pin_->attach_interrupt(RemoteReceiverComponentStore::gpio_intr, &this->store_, gpio::INTERRUPT_ANY_EDGE);
  ++this->recovery_count_;
  this->edge_activity_pending_ = false;
  this->last_edge_count_seen_ = this->store_.edge_count;
  if (log_warning) {
    const uint32_t now_ms = millis();
    if (this->last_recovery_log_ms_ == 0 ||
        static_cast<uint32_t>(now_ms - this->last_recovery_log_ms_) >= 5000) {
      ESP_LOGW(TAG, "RX gate: capture resynchronized after %s (recoveries=%lu)", reason,
               static_cast<unsigned long>(this->recovery_count_));
      this->last_recovery_log_ms_ = now_ms;
    }
  }
}

void RemoteReceiverComponent::setup() {
  if (this->capture_ready_ || this->is_failed()) return;
  this->pin_->setup();
  if (this->buffer_size_ < 3) {
    ESP_LOGE(TAG, "RX gate: invalid buffer size");
    this->mark_failed();
    return;
  }
  this->store_.idle_us = this->idle_us_;
  this->store_.filter_us = this->filter_us_;
  this->store_.pin = this->pin_->to_isr();
  this->store_.buffer = new (std::nothrow) int32_t[this->buffer_size_];
  if (this->store_.buffer == nullptr) {
    ESP_LOGE(TAG, "RX gate: buffer allocation failed");
    this->mark_failed();
    return;
  }
  this->store_.buffer_size = this->buffer_size_;
  this->reset_capture_state_();
  this->capture_ready_ = true;
  // A false initial setting NEVER installs the ISR, even momentarily.
  this->set_capture_enabled(this->capture_requested_);
}

void RemoteReceiverComponent::set_capture_enabled(bool enabled) {
  this->capture_requested_ = enabled;
  if (!this->capture_ready_ || this->is_failed() || enabled == this->capture_active_) return;
  if (enabled) {
    this->backlog_boost_active_ = false;
    this->backlog_boost_started_ms_ = 0;
    this->last_backlog_boost_stop_ms_ = 0;
    this->reset_capture_state_();
    this->capture_active_ = true;
    this->pin_->attach_interrupt(RemoteReceiverComponentStore::gpio_intr, &this->store_, gpio::INTERRUPT_ANY_EDGE);
    if (this->high_frequency_) this->high_freq_.start();
  } else {
    this->pin_->detach_interrupt();
    this->capture_active_ = false;
    this->backlog_boost_active_ = false;
    this->high_freq_.stop();
    this->reset_capture_state_();
  }
  ESP_LOGI(TAG, "RX gate: capture=%s; irq=%s; fast_loop=%s",
           enabled ? "ON" : "OFF", enabled ? "ON" : "OFF",
           this->is_high_frequency_requested() ? "ON" : "OFF");
}

void RemoteReceiverComponent::set_high_frequency(bool enabled) {
  if (this->high_frequency_ == enabled) return;
  this->high_frequency_ = enabled;
  if (this->capture_active_ && enabled) {
    // Explicit fast mode supersedes an adaptive boost without toggling the
    // requester off in between.
    this->backlog_boost_active_ = false;
    this->high_freq_.start();
  } else if (!this->backlog_boost_active_) {
    this->high_freq_.stop();
  }
  // No IRQ detach, buffer reset, pin-mode change or allocation here.
  if (this->capture_ready_) {
    ESP_LOGI(TAG, "RX scheduling: capture=%s; fast_loop=%s",
             this->capture_active_ ? "ON" : "OFF", this->is_high_frequency_requested() ? "ON" : "OFF");
  }
}

void RemoteReceiverComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Remote Receiver rxgate2 (ESP8266 / based on 2026.9.0):");
  ESP_LOGCONFIG(TAG, "  Capture enabled: %s", this->capture_active_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  High frequency configured: %s", this->high_frequency_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Adaptive backlog boost: bounded 8 ms / 20 ms cooldown when high_frequency=false");
  ESP_LOGCONFIG(TAG, "  Buffer Size: %" PRIu32, this->buffer_size_);
  ESP_LOGCONFIG(TAG, "  Filter: %" PRIu32 " us; Idle: %" PRIu32 " us", this->filter_us_, this->idle_us_);
  LOG_PIN("  Pin: ", this->pin_);
}

void RemoteReceiverComponent::loop() {
  if (!this->capture_active_ || this->is_failed()) return;
  ++this->loop_calls_;
  auto &s = this->store_;
  const uint32_t now_ms = millis();
  const uint32_t edges_now = s.edge_count;
  if (edges_now != this->last_edge_count_seen_) {
    if (!this->edge_activity_pending_) {
      this->edge_activity_pending_ = true;
      this->edge_activity_since_ms_ = now_ms;
    }
    this->last_edge_count_seen_ = edges_now;
  }
  // If RF edges keep arriving but a frame never reaches the normal 5 ms idle
  // boundary, the ring can remain wedged by noise/partial traffic. A supported
  // RFLink packet is far shorter than 2.5 s (including the long LaCrosse test),
  // so resynchronize only after this deliberately generous timeout.
  if (this->edge_activity_pending_ &&
      static_cast<uint32_t>(now_ms - this->edge_activity_since_ms_) >= 2500) {
    this->recover_capture_("stalled partial frame");
    return;
  }
  if (s.overflow) {
    ++this->overflow_reports_;
    ++this->overflow_log_pending_;
    s.overflow = false;
    const uint32_t now_ms = millis();
    // A noisy/continuous RF source can overflow repeatedly. Logging every loop
    // makes recovery worse on ESP8266, so keep the exact counter but aggregate
    // warnings to at most one line per 5 seconds.
    if (this->last_overflow_log_ms_ == 0 ||
        static_cast<uint32_t>(now_ms - this->last_overflow_log_ms_) >= 5000) {
      ESP_LOGW(TAG, "Buffer overflow (%lu since last log; total=%lu)",
               static_cast<unsigned long>(this->overflow_log_pending_),
               static_cast<unsigned long>(this->overflow_reports_));
      this->overflow_log_pending_ = 0;
      this->last_overflow_log_ms_ = now_ms;
    }
    // A ring overflow leaves the current packet undefined. Previously we only
    // cleared the flag, so stale indices could take several later transmissions
    // to converge. Re-arm immediately; this is the same cleanup users got from
    // manually toggling capture, without reallocating the buffer.
    this->recover_capture_("buffer overflow", false);
    return;
  }

  // Preserve legacy RFLink timing: deliver at most one completed RF frame per
  // receiver loop. If completed frames build up, temporarily ask ESPHome to
  // schedule more loop turns instead of decoding several frames in one call.
  this->update_backlog_boost_(now_ms);

  uint32_t last_index = s.buffer_start;
  if (last_index == s.buffer_read) {
    InterruptLock lock;
    if (s.buffer_read == s.buffer_start && s.buffer_write != s.buffer_start &&
        micros() - s.prev_micros >= this->idle_us_) {
      commit_value(&s, s.prev_micros, s.prev_level);
      write_value(&s, s.idle_us, !s.commit_level);
      last_index = s.buffer_start;
    }
  }
  if (last_index == s.buffer_read) return;
  uint32_t temp_read = s.buffer_read;
  uint32_t reserve_size = 0;
  while (temp_read != last_index && (uint32_t) std::abs(s.buffer[temp_read]) < this->idle_us_) {
    reserve_size++;
    temp_read++;
    if (temp_read >= s.buffer_size) temp_read = 0;
  }
  this->temp_.clear();
  this->temp_.reserve(reserve_size + 1);
  for (uint32_t i = 0; i < reserve_size + 1; i++) {
    this->temp_.push_back((int32_t) s.buffer[s.buffer_read++]);
    if (s.buffer_read >= s.buffer_size) s.buffer_read = 0;
  }
  ++this->frame_count_;
  this->edge_activity_pending_ = false;
  this->call_listeners_dumpers_();
  this->update_backlog_boost_(millis());
}
}  // namespace esphome::remote_receiver
