#pragma once
#include <cmath>
namespace esphome { namespace sensor {
class Sensor {
 public:
  void publish_state(float value) { state=value; }
  void set_internal(bool value) { internal=value; }
  bool is_internal() const { return internal; }
  bool internal{false};
  float state{NAN};
};
} }
