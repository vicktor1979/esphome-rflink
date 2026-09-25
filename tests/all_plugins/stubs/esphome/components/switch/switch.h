#pragma once
#include <optional>
#define LOG_SWITCH(prefix, type, obj) ((void)0)
namespace esphome { namespace switch_ {
class Switch {
 public:
  virtual ~Switch() = default;
  void publish_state(bool value) { this->state = value; }
  std::optional<bool> get_initial_state_with_restore_mode() const { return std::nullopt; }
  bool state{false};
 protected:
  virtual void write_state(bool) {}
};
} }
