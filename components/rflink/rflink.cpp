#include "rflink.h"
#include "rflink_engine.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rflink {
static const char *const TAG = "rflink";
void RFLinkComponent::setup() { ::rflink_legacy::reset(); }
void RFLinkComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RFLink RX compatibility bridge v0.1:");
  ESP_LOGCONFIG(TAG, "  RX plugins compiled: %u", static_cast<unsigned>(::rflink_legacy::plugin_count()));
  ESP_LOGCONFIG(TAG, "  Arduino framework; original plugin sources; TX not implemented");
}
bool RFLinkComponent::on_receive(remote_base::RemoteReceiveData data) {
  std::string json;
  bool recognized = ::rflink_legacy::decode(data.get_raw_data(), json);
  if (!json.empty()) {
    if (this->log_messages_)
      ESP_LOGD(TAG, "%s", json.c_str());
    // Triggered for EVERY emitted frame, independently of text_sensor state.
    this->callbacks_.call(json);
  }
  return recognized;
}
}  // namespace rflink
}  // namespace esphome
