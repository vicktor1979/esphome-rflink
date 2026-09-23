
#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cassert>
#include <algorithm>
namespace esphome { namespace event {
class Event {
 public:
  std::vector<std::string> types, events;
  std::function<void(const std::string&)> callback;
  void set_event_types(std::initializer_list<const char*> t){types.assign(t.begin(),t.end());}
  void trigger(const std::string& t){assert(std::find(types.begin(),types.end(),t)!=types.end());events.push_back(t);if(callback)callback(t);}
  int count(const std::string& t)const{return std::count(events.begin(),events.end(),t);}
};
} }
