// Host-only shim, never shipped into the ESPHome firmware build.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <algorithm>
using byte = uint8_t;
using boolean = bool;
using String = std::string;
using word = uint16_t;
extern uint32_t test_millis;
inline unsigned long millis() { return test_millis; }
inline unsigned long micros() { return test_millis * 1000UL; }
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned int) {}
inline void yield() {}
#define PROGMEM
#define PSTR(x) (x)
#define F(x) (x)
#define sprintf_P std::sprintf
#define snprintf_P std::snprintf
#define memcpy_P std::memcpy
#define strcpy_P std::strcpy
#define pgm_read_byte(x) (*(const uint8_t *)(x))
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#define HEX 16
#define DEC 10
#define LOW 0
#define HIGH 1
struct SerialStub {
  template<class... T> void print(T...) {}
  template<class... T> void println(T...) {}
  template<class... T> void printf(T...) {}
  template<class... T> void write(T...) {}
};
inline SerialStub Serial;
