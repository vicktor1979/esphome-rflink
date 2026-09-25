
#pragma once
namespace esphome { namespace binary_sensor {
class BinarySensor {public:bool state=false,have=false; bool has_state()const{return have;} void publish_state(bool s){state=s;have=true;} };
} }
