#pragma once
#include <cstdint>
#include <vector>
namespace esphome { namespace remote_base {
class RemoteReceiveData {const std::vector<int32_t>& raw_;
 public: explicit RemoteReceiveData(const std::vector<int32_t>& r):raw_(r){}
 const std::vector<int32_t>& get_raw_data() const { return raw_; }
};
class RemoteReceiverListener {public: virtual ~RemoteReceiverListener()=default; virtual bool on_receive(RemoteReceiveData)=0;};
} }
