// Copyright (c) 2019 ESPHome. SPDX-License-Identifier: GPL-3.0-only
// Derived from ESPHome 2026.9.0 remote_receiver.cpp. Modified 2026-09-23.
// The original ESP8266 pulse collection algorithm is retained. Additions:
// pre-setup capture gate, pin-IRQ detach/reattach, ring reset, edge counter.
// rxgate2: independently configurable high-frequency-loop request.
// ISR/filter/timestamping/frame reconstruction remain upstream-compatible;
// main-loop draining adds bounded backlog catch-up for high_frequency:false.
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


void RemoteReceiverComponent::recover_capture_(const char *reason, bool log_warning) {
  if (!this->capture_ready_ || !this->capture_active_ || this->is_failed()) return;
  // The ring contents are already unusable after overflow/stall. Re-arm only
  // our GPIO interrupt; do not touch Wi-Fi/system interrupts or reallocate RAM.
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
    this->reset_capture_state_();
    this->capture_active_ = true;
    this->pin_->attach_interrupt(RemoteReceiverComponentStore::gpio_intr, &this->store_, gpio::INTERRUPT_ANY_EDGE);
    if (this->high_frequency_) this->high_freq_.start();
  } else {
    this->pin_->detach_interrupt();
    this->capture_active_ = false;
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
    this->high_freq_.start();
  } else {
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

  // With high_frequency:false ESPHome may service component loops at roughly a
  // 16 ms cadence. The upstream receiver consumes only one completed RF frame
  // per loop; a noisy 433 MHz input that produces completed frames only a little
  // faster than that can therefore fill the ring slowly (the field failure was
  // reproducible at ~92 s with a 1200-entry buffer). Drain a small bounded batch
  // of already-completed frames per scheduler turn. This raises burst capacity
  // without restoring the always-fast loop that previously hurt ESP8266 Wi-Fi.
  static constexpr uint8_t MAX_FRAMES_PER_LOOP = 4;
  static constexpr uint32_t MAX_DRAIN_TIME_US = 6000;
  const uint32_t drain_started_us = micros();
  uint8_t drained_this_loop = 0;

  while (drained_this_loop < MAX_FRAMES_PER_LOOP) {
    // The first frame is always serviced. Before taking any additional queued
    // frame, enforce the elapsed-work budget so a slow decoder/callback cannot
    // turn backlog catch-up into another Wi-Fi starvation source.
    if (drained_this_loop != 0 &&
        static_cast<uint32_t>(micros() - drain_started_us) >= MAX_DRAIN_TIME_US)
      break;

    if (s.overflow) {
      ++this->overflow_reports_;
      ++this->overflow_log_pending_;
      s.overflow = false;
      const uint32_t overflow_now_ms = millis();
      // A noisy/continuous RF source can overflow repeatedly. Logging every loop
      // makes recovery worse on ESP8266, so keep the exact counter but aggregate
      // warnings to at most one line per 5 seconds.
      if (this->last_overflow_log_ms_ == 0 ||
          static_cast<uint32_t>(overflow_now_ms - this->last_overflow_log_ms_) >= 5000) {
        ESP_LOGW(TAG, "Buffer overflow (%lu since last log; total=%lu)",
                 static_cast<unsigned long>(this->overflow_log_pending_),
                 static_cast<unsigned long>(this->overflow_reports_));
        this->overflow_log_pending_ = 0;
        this->last_overflow_log_ms_ = overflow_now_ms;
      }
      // A ring overflow leaves the current packet undefined. Previously we only
      // cleared the flag, so stale indices could take several later transmissions
      // to converge. Re-arm immediately; this is the same cleanup users got from
      // manually toggling capture, without reallocating the buffer.
      this->recover_capture_("buffer overflow", false);
      return;
    }

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
    if (last_index == s.buffer_read) break;

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
    ++drained_this_loop;
    if (drained_this_loop > 1) ++this->extra_drained_frames_;
    if (drained_this_loop > this->max_drain_batch_) this->max_drain_batch_ = drained_this_loop;
    this->edge_activity_pending_ = false;
    this->call_listeners_dumpers_();
  }
}

}  // namespace esphome::remote_receiver
