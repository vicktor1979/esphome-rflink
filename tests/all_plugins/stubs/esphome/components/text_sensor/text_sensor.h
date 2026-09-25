
#pragma once
#include <string>
#include <vector>
namespace esphome { namespace text_sensor {
class TextSensor {public:std::string state;std::vector<std::string> history;void publish_state(const std::string& s){state=s;history.push_back(s);} };
} }
