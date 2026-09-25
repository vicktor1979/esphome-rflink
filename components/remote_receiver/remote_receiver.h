// Copyright (c) 2019 ESPHome. SPDX-License-Identifier: GPL-3.0-only
// Derived from ESPHome 2026.9.0 remote_receiver.h. Modified 2026-09-23.
// ESP8266-only diagnostic: pause capture IRQ + fast-loop request, not only decode.
#pragma once
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"
#include <cinttypes>

#ifndef USE_ESP8266
#error "RFLink rxgate2 external remote_receiver supports ESP8266 only"
#endif

namespace esphome::remote_receiver {
struct RemoteReceiverComponentStore {
  static void gpio_intr(RemoteReceiverComponentStore *arg);
  volatile int32_t *buffer{nullptr};
  volatile uint32_t buffer_write{0};
  volatile uint32_t buffer_start{0};
  uint32_t buffer_read{0};
  volatile uint32_t commit_micros{0};
  volatile uint32_t prev_micros{0};
  uint32_t buffer_size{1000};
  uint32_t filter_us{10};
  uint32_t idle_us{10000};
  ISRInternalGPIOPin pin;
  volatile bool commit_level{false};
  volatile bool prev_level{false};
  volatile bool overflow{false};
  volatile uint32_t edge_count{0};  // diagnostic; cumulative, including filtered edges
};

class RemoteReceiverComponent final : public remote_base::RemoteReceiverBase, public Component {
 public:
  explicit RemoteReceiverComponent(InternalGPIOPin *pin) : RemoteReceiverBase(pin) {}
  void setup() override;
  void dump_config() override;
  void loop() override;
  void on_shutdown() override { this->set_capture_enabled(false); }
  void set_buffer_size(uint32_t value) { this->buffer_size_ = value; }
  void set_filter_us(uint32_t value) { this->filter_us_ = value; }
  void set_idle_us(uint32_t value) { this->idle_us_ = value; }
  // Main-loop/setup context only. Before setup this stores the initial setting.
  void set_capture_enabled(bool enabled);
  bool is_capture_enabled() const { return this->capture_active_; }
  // Main-loop/setup context only. Does not detach IRQ or reset pulse buffers.
  void set_high_frequency(bool enabled);
  bool is_high_frequency_requested() const { return this->capture_active_ && this->high_frequency_; }
  uint32_t get_loop_calls() const { return this->loop_calls_; }
  // Counts overflow flags observed by loop(), not lost edges or lost packets.
  uint32_t get_overflow_reports() const { return this->overflow_reports_; }
  uint32_t get_edge_count() const { return this->store_.edge_count; }
  uint32_t get_frame_count() const { return this->frame_count_; }
  uint32_t get_recovery_count() const { return this->recovery_count_; }
  uint32_t get_extra_drained_frames() const { return this->extra_drained_frames_; }
  uint8_t get_max_drain_batch() const { return this->max_drain_batch_; }

 protected:
  void reset_capture_state_();  // call ONLY while our pin interrupt is detached
  void recover_capture_(const char *reason, bool log_warning = true);
  RemoteReceiverComponentStore store_;
  HighFrequencyLoopRequester high_freq_;
  uint32_t buffer_size_{1000};
  uint32_t filter_us_{10};
  uint32_t idle_us_{10000};
  bool high_frequency_{true};  // backwards-compatible unless YAML opts out
  uint32_t loop_calls_{0};
  uint32_t overflow_reports_{0};
  uint32_t overflow_log_pending_{0};
  uint32_t last_overflow_log_ms_{0};
  uint32_t frame_count_{0};
  uint32_t recovery_count_{0};
  uint32_t extra_drained_frames_{0};
  uint8_t max_drain_batch_{0};
  uint32_t last_recovery_log_ms_{0};
  uint32_t last_edge_count_seen_{0};
  uint32_t edge_activity_since_ms_{0};
  bool edge_activity_pending_{false};
  bool capture_ready_{false};
  bool capture_requested_{true};
  bool capture_active_{false};
};
}  // namespace esphome::remote_receiver
