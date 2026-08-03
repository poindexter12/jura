#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace jura_coolcontrol {

static const char *const TAG = "jura_coolcontrol";

class JuraCoolcontrol : public PollingComponent, public uart::UARTDevice {
 public:
  void set_level_sensor(sensor::Sensor *s) { this->level_sensor_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_sensor_ = s; }

  // The CoolControl broadcasts its status continuously as plain-text frames
  // terminated by CRLF; we only ever listen. Frames are assembled here and
  // the latest complete one is kept for update() to publish.
  void loop() override {
    while (this->available()) {
      line_.push_back(static_cast<char>(this->read()));
      const size_t n = line_.size();
      if (n >= 2 && line_[n - 2] == '\r' && line_[n - 1] == '\n') {
        line_.resize(n - 2);
        if (parse_frame_(line_)) last_frame_at_ = millis();
        line_.clear();
      } else if (n > 256) {
        ESP_LOGW(TAG, "Discarding oversized frame");
        line_.clear();
      }
    }
  }

  void update() override {
    if (last_frame_at_ == 0) {
      ESP_LOGW(TAG, "No broadcast received from CoolControl yet");
      return;
    }
    if (millis() - last_frame_at_ > STALE_AFTER_MS) {
      ESP_LOGW(TAG, "No broadcast for %u ms; not publishing stale values",
               (unsigned) (millis() - last_frame_at_));
      return;
    }
    if (this->level_sensor_) this->level_sensor_->publish_state(level_);
    if (this->temperature_sensor_) this->temperature_sensor_->publish_state(temp_c_);
  }

 protected:
  static const uint32_t STALE_AFTER_MS = 15000;

  bool parse_frame_(const std::string &frame) {
    if (frame.size() < 8) {
      ESP_LOGD(TAG, "Ignoring short frame (len=%d): %s", (int) frame.size(), frame.c_str());
      return false;
    }
    const uint8_t level = static_cast<uint8_t>(strtol(frame.substr(4, 2).c_str(), nullptr, 16));
    const uint8_t temp_raw = static_cast<uint8_t>(strtol(frame.substr(6, 2).c_str(), nullptr, 16));
    level_ = level;
    temp_c_ = temp_raw / 10.0f;
    ESP_LOGD(TAG, "Frame: %s (level=%.0f%% temp=%.1f°C)", frame.c_str(), level_, temp_c_);
    return true;
  }

  sensor::Sensor *level_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};

  std::string line_;
  uint32_t last_frame_at_{0};
  float level_{0};
  float temp_c_{0};
};

}  // namespace jura_coolcontrol
}  // namespace esphome
