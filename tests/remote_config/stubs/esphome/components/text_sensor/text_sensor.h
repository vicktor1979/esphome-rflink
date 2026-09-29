
#pragma once
#include <string>
#include <vector>
namespace esphome { namespace text_sensor {
class TextSensor {
 public:
  std::string state;
  std::vector<std::string> history;
  bool have{false}, internal{false};
  void publish_state(const std::string &s) { state=s; have=true; history.push_back(s); }
  bool has_state() const { return have; }
  void set_internal(bool value) { internal=value; }
  bool is_internal() const { return internal; }
};
} }
