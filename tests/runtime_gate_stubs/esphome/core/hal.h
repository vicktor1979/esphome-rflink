#pragma once
#include <cstdint>
extern uint32_t probe_clock;
namespace esphome { inline uint32_t micros() {auto now=probe_clock; probe_clock+=10; return now;} }
