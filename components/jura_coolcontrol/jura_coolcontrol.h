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

// Sends 'out' plus CRLF using Jura's bit placement, then reads until CRLF or timeout.
std::string cmd2jura(const std::string &out, uint32_t rx_timeout_ms = 1000) {
  // 1) Flush any stale input
  while (this->available()) this->read();

  // 3) RX: bounded by timeout
  std::string inbytes;
  inbytes.reserve(64);
  uint8_t assembled = 0;
  int bitidx = 0;

  const uint32_t start = millis();
  while (millis() - start < rx_timeout_ms) {
    if (this->available()) {
      int raw = this->read();
      //ESP_LOGI(TAG, "RAW: %i,%c", raw, raw);      
      inbytes.push_back(static_cast<char>(raw));

      // CRLF terminator?
      const size_t n = inbytes.size();
      if (n >= 2 && inbytes[n - 2] == '\r' && inbytes[n - 1] == '\n') {
        inbytes.resize(n - 2);  // strip CRLF
        return inbytes;
      }

      // guard against runaway data
      if (inbytes.size() > 2048) {
        ESP_LOGW(TAG, "RX buffer exceeded 2KB; truncating and returning partial.");
        return inbytes;
      }
    } else {
      delay(1);  // brief yield
    }
  }
  // Timeout: return whatever we have (or {} if you prefer)
  return inbytes;
}

  void update() override {
      // Send the configured command and log the parsed response
      std::string resp = this->cmd2jura("RT:0000");
      if (resp.size() >= 8) {
        ESP_LOGI(TAG, "Response (text): %s", resp.c_str());

        uint8_t value = static_cast<uint8_t>(strtol(resp.substr(4, 2).c_str(), nullptr, 16));
        ESP_LOGI(TAG, "Level: %i", value);
        float level = value;
        value = static_cast<uint8_t>(strtol(resp.substr(6, 2).c_str(), nullptr, 16));
        ESP_LOGI(TAG, "Temperature: %i", value);
        float temp_c = value / 10.0;

        if (this->level_sensor_ && !std::isnan(level)) {
          this->level_sensor_->publish_state(level);
        }
        if (this->temperature_sensor_ && !std::isnan(temp_c)) {
          this->temperature_sensor_->publish_state(temp_c);
        }
      } else if (!resp.empty()) {
        ESP_LOGW(TAG, "Response too short to parse (len=%d): %s", (int) resp.size(), resp.c_str());
      } else {
        ESP_LOGW(TAG, "No response (timeout or empty).");
      }
    }
  protected:    
   sensor::Sensor *level_sensor_{nullptr};
   sensor::Sensor *temperature_sensor_{nullptr};
};

}  // namespace jura_coolcontrol
}  // namespace esphome
