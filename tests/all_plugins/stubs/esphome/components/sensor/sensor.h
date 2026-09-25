#pragma once
#include <cmath>
namespace esphome { namespace sensor {
class Sensor {
 public:
  void publish_state(float value) { state=value; }
  float state{NAN};
};
} }
