#include "rflink_sensor.h"
#include <cstring>
#include "esphome/components/json/json_util.h"
#include "esphome/core/log.h"

namespace esphome { namespace rflink_sensor {
static const char *const TAG = "rflink.sensor";
// RF IDs are matched as text, preserving leading zeroes. ASCII hex case is ignored.
static bool id_matches(const std::string &expected, const char *actual) {
  if (actual == nullptr) return false;
  auto fold = [](char c) { return c >= 'A' && c <= 'Z' ? char(c + ('a' - 'A')) : c; };
  size_t i = 0;
  for (; i < expected.size(); ++i) {
    if (actual[i] == '\0' || fold(expected[i]) != fold(actual[i])) return false;
  }
  return actual[i] == '\0';
}
void RFLinkSensor::setup() {
  this->parent_->add_on_message_observer([this](const std::string &message) { this->on_message_(message); });
}
void RFLinkSensor::dump_config() {
  LOG_SENSOR("", "RFLink sensor", this);
  ESP_LOGCONFIG(TAG, "  Protocol: %s; RF ID: %s; field: %s", this->protocol_.c_str(),
                this->rf_id_.c_str(), this->battery_field_ ? "BAT (LOW=0, OK=100)" : rflink_data::FIELDS[this->field_].key);
}
void RFLinkSensor::on_message_(const std::string &message) {
  // Bridge JSON has literal field keys. EV button messages lack TEMP/HUM etc.,
  // so they take this allocation-free path without an additional JSON parse.
  if (message.find(this->field_token_) == std::string::npos) return;
  json::parse_json(message, [this](JsonObject root) -> bool {
    const char *protocol = root["NAME"] | "";
    const char *rf_id = root["ID"] | "";
    if (this->protocol_ != protocol || !id_matches(this->rf_id_, rf_id)) return true;
    if (this->battery_field_) {
      // The original formatter emits only LOW / OK, not a percentage or
      // NORMAL/HIGH. Never convert absent/unknown states into a healthy battery.
      const char *battery = root["BAT"] | "";
      if (std::strcmp(battery, "LOW") == 0) this->publish_state(0.0f);
      else if (std::strcmp(battery, "OK") == 0) this->publish_state(100.0f);
      return true;
    }
    const auto value = rflink_data::number(root, this->field_);
    // Missing/invalid fields retain the last reading. No inferred values,
    // learning gate or cooldown. Standard ESPHome filters remain available.
    if (value.valid && std::isfinite(value.value)) this->publish_state(value.value);
    return true;
  });
}
}}  // namespace esphome::rflink_sensor
