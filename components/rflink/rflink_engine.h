#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rflink_legacy {
void reset();
size_t plugin_count();
// Returns recognized=true even when an RFLink plugin suppresses a duplicate.
// Only a nonempty output JSON represents an event that should be published.
bool decode(const std::vector<int32_t> &timings, std::string &json);
}  // namespace rflink_legacy
