#pragma once
#include <cstdint>
#define IRAM_ATTR
#define HOT
extern uint32_t capture_clock;
namespace esphome { inline uint32_t micros() { return capture_clock; } inline uint32_t millis() { return capture_clock / 1000U; } }
