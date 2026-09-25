#pragma once
#include <cstdint>
struct ESPClass {
 uint32_t getFreeHeap() const { return 24000; }
 uint32_t getMaxFreeBlockSize() const { return 20000; }
 uint8_t getHeapFragmentation() const { return 12; }
};
inline ESPClass ESP;
