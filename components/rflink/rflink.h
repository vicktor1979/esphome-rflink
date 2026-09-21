#pragma once
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
  void add_on_message_callback(std::function<void(std::string)> &&callback) {
    this->callbacks_.add(std::move(callback));
  }
 protected:
  bool log_messages_{true};
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
