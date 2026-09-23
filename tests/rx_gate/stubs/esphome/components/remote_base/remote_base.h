#pragma once
#include <cstdint>
#include <vector>
#include <functional>
namespace esphome {
namespace gpio { enum InterruptType { INTERRUPT_ANY_EDGE }; }
class InternalGPIOPin;
class ISRInternalGPIOPin {
 public:
  ISRInternalGPIOPin()=default;
  explicit ISRInternalGPIOPin(InternalGPIOPin *pin): pin_(pin) {}
  bool digital_read() const;
 private: InternalGPIOPin *pin_=nullptr;
};
class InternalGPIOPin {
 public:
  bool level=false;
  unsigned attachments=0,detachments=0,setups=0,edge_deliveries=0;
  void setup(){ ++setups; }
  bool digital_read() const { return level; }
  ISRInternalGPIOPin to_isr() { return ISRInternalGPIOPin(this); }
  template<class T> void attach_interrupt(void (*fn)(T*), T *arg, gpio::InterruptType) {
    callback_=[=](){fn(arg);}; ++attachments;
  }
  void detach_interrupt() { callback_={}; ++detachments; }
  bool attached() const { return bool(callback_); }
  void set_level(bool value) {
    if(value==level) return;
    level=value;
    if(callback_) { ++edge_deliveries; callback_(); }
  }
 private: std::function<void()> callback_;
};
inline bool ISRInternalGPIOPin::digital_read() const { return pin_->digital_read(); }
namespace remote_base {
enum ToleranceMode { TOLERANCE_MODE_PERCENTAGE, TOLERANCE_MODE_TIME };
class RemoteReceiveData {
 const std::vector<int32_t>& raw_;
 public: explicit RemoteReceiveData(const std::vector<int32_t>& raw):raw_(raw){}
 const std::vector<int32_t>& get_raw_data() const { return raw_; }
};
class RemoteReceiverListener {
 public: virtual ~RemoteReceiverListener()=default;
 virtual bool on_receive(RemoteReceiveData)=0;
};
class RemoteReceiverBase {
 public:
  explicit RemoteReceiverBase(InternalGPIOPin *pin):pin_(pin){}
  virtual ~RemoteReceiverBase()=default;
  void register_listener(RemoteReceiverListener *l){listeners_.push_back(l);}
  void set_tolerance(uint32_t v,ToleranceMode mode){tolerance_=v;tolerance_mode_=mode;}
 protected:
  void call_listeners_dumpers_(){ for(auto *l:listeners_)l->on_receive(RemoteReceiveData(temp_)); }
  InternalGPIOPin *pin_;
  std::vector<int32_t> temp_;
  uint32_t tolerance_=25;
  ToleranceMode tolerance_mode_=TOLERANCE_MODE_PERCENTAGE;
 private: std::vector<RemoteReceiverListener*> listeners_;
};
}
}
