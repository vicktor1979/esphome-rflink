#pragma once
#include <functional>
#include <vector>
#include <utility>
namespace esphome {
class Component { public: virtual ~Component() = default; virtual void setup() {} virtual void dump_config() {} virtual void loop() {} void mark_failed() {failed_=true;} bool failed_=false; };
template<class Signature> class CallbackManager;
template<class... Args> class CallbackManager<void(Args...)> {
 std::vector<std::function<void(Args...)>> callbacks_;
 public: void add(std::function<void(Args...)>&& f) { callbacks_.push_back(std::move(f)); }
 void call(Args... a) { for(auto& f:callbacks_) f(a...); }
};
}
