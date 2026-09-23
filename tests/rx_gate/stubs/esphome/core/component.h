#pragma once
#include <cmath>
#include <functional>
#include <vector>
#include <utility>
#include "helpers.h"
namespace esphome {
class Component {
 public:
  virtual ~Component()=default;
  virtual void setup() {} virtual void loop() {} virtual void dump_config() {}
  virtual void on_shutdown() {}
  bool is_failed() const { return failed_; }
  void mark_failed() { failed_=true; }
 private: bool failed_=false;
};
template<class Signature> class CallbackManager;
template<class... Args> class CallbackManager<void(Args...)> {
 std::vector<std::function<void(Args...)>> callbacks_;
 public: void add(std::function<void(Args...)>&& f) { callbacks_.push_back(std::move(f)); }
 void call(Args... a) { for(auto& f:callbacks_) f(a...); }
};
}
