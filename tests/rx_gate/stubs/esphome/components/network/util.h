#pragma once
namespace esphome::network {
inline bool test_network_connected=false;
inline bool is_connected() { return test_network_connected; }
}
