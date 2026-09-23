#pragma once
namespace esphome {
struct InterruptLock { InterruptLock() {} ~InterruptLock() {} };
class HighFrequencyLoopRequester {
 public:
  static inline unsigned requests=0;
  void start() { if (!active_) { active_=true; ++requests; } }
  void stop() { if (active_) { active_=false; --requests; } }
  static bool is_high_frequency() { return requests!=0; }
 private: bool active_=false;
};
}
