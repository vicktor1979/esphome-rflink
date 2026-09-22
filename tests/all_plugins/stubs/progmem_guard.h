// Linux host-only PSTR access regression shim. NOT an ESP8266 emulator.
// PSTR pointers refer to inaccessible pages. Only read_byte()/the _P helpers
// can see the backing bytes. A direct *p read therefore fails immediately.
// This intentionally models string access only, not all PROGMEM objects.
#pragma once
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

namespace progmem_guard {
struct Region {
  char *address;
  size_t mapping_size;
  std::string contents;
};
struct Storage {
  std::vector<Region> regions;
  size_t reads = 0;
  ~Storage() {
    for (const auto &region : regions) munmap(region.address, region.mapping_size);
  }
};
inline Storage &storage() { static Storage state; return state; }
inline char *store(const char *text) {
  const std::string contents(text);
  for (const auto &region : storage().regions)
    if (region.contents == contents) return region.address;
  const long page = sysconf(_SC_PAGESIZE);
  if (page <= 0) throw std::runtime_error("Cannot get page size");
  const size_t page_size = static_cast<size_t>(page);
  const size_t size = ((contents.size() + 1 + page_size - 1) / page_size) * page_size;
  void *address = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (address == MAP_FAILED) throw std::runtime_error("Cannot map PROGMEM guard page");
  storage().regions.push_back({static_cast<char *>(address), size, contents});
  return static_cast<char *>(address);
}
inline uint8_t read_byte(const void *pointer) {
  const auto addr = reinterpret_cast<uintptr_t>(pointer);
  for (const auto &region : storage().regions) {
    const auto base = reinterpret_cast<uintptr_t>(region.address);
    if (addr >= base && addr - base < region.mapping_size) {
      const size_t offset = addr - base;
      if (offset > region.contents.size())
        throw std::out_of_range("Read beyond PROGMEM string terminator");
      ++storage().reads;
      return static_cast<uint8_t>(region.contents[offset]);
    }
  }
  return *static_cast<const uint8_t *>(pointer); // Normal RAM buffer.
}
inline std::string copy_string(const char *source) {
  std::string result;
  for (;; ++source) {
    const uint8_t value = read_byte(source);
    if (value == 0) return result;
    result.push_back(static_cast<char>(value));
  }
}
inline void *copy_memory(void *destination, const void *source, size_t count) {
  auto *out = static_cast<uint8_t *>(destination);
  const auto *in = static_cast<const uint8_t *>(source);
  for (size_t i = 0; i < count; ++i) out[i] = read_byte(in + i);
  return destination;
}
inline char *copy_c_string(char *destination, const char *source) {
  const std::string copy = copy_string(source);
  std::memcpy(destination, copy.c_str(), copy.size() + 1);
  return destination;
}
// Only the format is a _P argument in these helpers; string variadic arguments
// must be RAM. This suffices for the RX fixtures used in these tests.
template<class... Args>
inline int sprintf_format(char *destination, const char *format, Args... args) {
  const std::string copy = copy_string(format);
  return std::sprintf(destination, copy.c_str(), args...);
}
template<class... Args>
inline int snprintf_format(char *destination, size_t size, const char *format, Args... args) {
  const std::string copy = copy_string(format);
  return std::snprintf(destination, size, copy.c_str(), args...);
}
}  // namespace progmem_guard
