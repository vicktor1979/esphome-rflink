
#pragma once
namespace esphome { namespace binary_sensor {
class BinarySensor {
 public:
  bool state{false}, have{false}, internal{false};
  bool has_state() const { return have; }
  void publish_state(bool value) { state=value; have=true; }
  void invalidate_state() { have=false; }
  void set_internal(bool value) { internal=value; }
  bool is_internal() const { return internal; }
};
} }
