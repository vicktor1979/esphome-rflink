#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/rflink/rflink.h"

namespace esphome { namespace rflink_sensor {
class RFLinkSensor : public sensor::Sensor, public Component {
 public:
  void set_parent(rflink::RFLinkComponent *parent) { this->parent_ = parent; }
  void set_match(const std::string &protocol, const std::string &rf_id) {
    this->protocol_ = protocol;
    this->rf_id_ = rf_id;
  }
  void set_field(rflink_data::Field field) {
    this->battery_field_ = false;
    this->field_ = field;
    this->field_token_ = std::string("\"") + rflink_data::FIELDS[field].key + "\"";
  }
  void set_battery_field() {
    this->battery_field_ = true;
    this->field_token_ = "\"BAT\"";
  }
  void setup() override;
  void dump_config() override;
 protected:
  void on_message_(const std::string &message);
  rflink::RFLinkComponent *parent_{nullptr};
  rflink_data::Field field_{rflink_data::TEMP};
  bool battery_field_{false};
  std::string protocol_, rf_id_, field_token_{"\"TEMP\""};
};
}}  // namespace esphome::rflink_sensor
