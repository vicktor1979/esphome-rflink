// Copyright (c) 2019 ESPHome. SPDX-License-Identifier: GPL-3.0-only
// Derived from ESPHome 2026.9.0 remote_receiver.h. Modified 2026-09-23.
// ESP8266-only diagnostic: pause capture IRQ + fast-loop request, not only decode.
#pragma once
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"
#include <cinttypes>

#ifndef USE_ESP8266
#error "RFLink rxgate1 external remote_receiver supports ESP8266 only"
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
  uint32_t get_edge_count() const { return this->store_.edge_count; }

 protected:
  void reset_capture_state_();  // call ONLY while our pin interrupt is detached
  RemoteReceiverComponentStore store_;
  HighFrequencyLoopRequester high_freq_;
  uint32_t buffer_size_{1000};
  uint32_t filter_us_{10};
  uint32_t idle_us_{10000};
  bool capture_ready_{false};
  bool capture_requested_{true};
  bool capture_active_{false};
};
}  // namespace esphome::remote_receiver
