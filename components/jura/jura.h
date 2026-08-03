#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/log.h"
#include <deque>
#include <map>
#include <vector>

namespace esphome {
namespace jura {

static const char *const TAG = "jura";

class Jura : public PollingComponent, public uart::UARTDevice {
 public:
  void set_model(const std::string &m) { model_ = m; }

  void register_metric_sensor(const std::string &key, sensor::Sensor *s) { numeric_[key].push_back(s); }
  void register_text_sensor(const std::string &key, text_sensor::TextSensor *t) { text_[key] = t; }

  // Maintenance binary sensor: true once counter_n reaches threshold.
  void add_maintenance_sensor(binary_sensor::BinarySensor *bs, int counter_n, long threshold) {
    maintenance_.push_back({bs, counter_n, threshold});
  }

  void publish_number(const std::string &key, float value) {
    auto it = numeric_.find(key);
    if (it == numeric_.end()) return;
    for (auto *s : it->second) {
      if (s) s->publish_state(value);
    }
  }
  void publish_text(const std::string &key, const std::string &value) {
    auto it = text_.find(key);
    if (it != text_.end() && it->second) it->second->publish_state(value);
  }

  // Queue a command for transmission; the response is handled asynchronously
  // in loop() and logged. Polls and user commands share one queue, so they
  // never interleave on the wire.
  void send_command(const std::string &cmd) { enqueue_(cmd, REQ_USER); }

  // Deprecated: use send_command(). Kept so existing lambdas compile; the
  // command is queued and the (asynchronous) response can no longer be
  // returned, so this always returns "".
  std::string cmd2jura(const std::string &cmd) {
    send_command(cmd);
    return "";
  }

  void update() override {
    if (!type_received_) enqueue_("TY:", REQ_TYPE);
    enqueue_("RT:0000", REQ_COUNTERS);
    enqueue_("IC:", REQ_FLAGS);
  }

  void loop() override {
    switch (state_) {
      case STATE_IDLE:
        if (!queue_.empty()) start_tx_();
        break;
      case STATE_TX:
        run_tx_();
        break;
      case STATE_RX:
        run_rx_();
        break;
    }
  }

 protected:
  enum RequestKind : uint8_t { REQ_COUNTERS, REQ_FLAGS, REQ_TYPE, REQ_USER };
  struct Request {
    std::string cmd;
    RequestKind kind;
  };
  enum State : uint8_t { STATE_IDLE, STATE_TX, STATE_RX };

  static const uint32_t CHAR_GAP_MS = 8;
  static const uint32_t RESPONSE_TIMEOUT_MS = 3000;
  static const size_t QUEUE_MAX = 8;
  static const size_t RX_MAX = 512;

  void enqueue_(const std::string &cmd, RequestKind kind) {
    if (kind != REQ_USER) {
      // one queued/in-flight poll of each kind is enough
      if (state_ != STATE_IDLE && active_.kind == kind) return;
      for (const auto &r : queue_) {
        if (r.kind == kind) return;
      }
    }
    if (queue_.size() >= QUEUE_MAX) {
      ESP_LOGW(TAG, "Command queue full, dropping %s", cmd.c_str());
      return;
    }
    queue_.push_back({cmd, kind});
  }

  void start_tx_() {
    active_ = queue_.front();
    queue_.pop_front();
    while (available()) read();  // flush stale input
    tx_buf_ = active_.cmd + "\r\n";
    tx_pos_ = 0;
    next_char_at_ = millis();
    state_ = STATE_TX;
  }

  // Jura encoding: each data byte is spread over four UART bytes carrying two
  // bits each (at bit positions 2 and 5), with an 8 ms gap between characters.
  void run_tx_() {
    const uint32_t now = millis();
    if ((int32_t) (now - next_char_at_) < 0) return;
    const uint8_t src = static_cast<uint8_t>(tx_buf_[tx_pos_]);
    for (int s = 0; s < 8; s += 2) {
      uint8_t raw = 0xFF;
      raw = (raw & ~(1u << 2)) | (((src >> s) & 1u) << 2);
      raw = (raw & ~(1u << 5)) | (((src >> (s + 1)) & 1u) << 5);
      write(raw);
    }
    if (++tx_pos_ >= tx_buf_.size()) {
      rx_buf_.clear();
      rx_bits_ = 0;
      rx_byte_ = 0;
      deadline_ = now + RESPONSE_TIMEOUT_MS;
      state_ = STATE_RX;
    } else {
      next_char_at_ = now + CHAR_GAP_MS;
    }
  }

  void run_rx_() {
    while (available()) {
      const uint8_t raw = static_cast<uint8_t>(read());
      rx_byte_ |= ((raw >> 2) & 1u) << rx_bits_;
      rx_byte_ |= ((raw >> 5) & 1u) << (rx_bits_ + 1);
      if ((rx_bits_ += 2) >= 8) {
        rx_buf_.push_back(static_cast<char>(rx_byte_));
        rx_bits_ = 0;
        rx_byte_ = 0;
        const size_t n = rx_buf_.size();
        if (n >= 2 && rx_buf_[n - 2] == '\r' && rx_buf_[n - 1] == '\n') {
          rx_buf_.resize(n - 2);
          handle_response_();
          state_ = STATE_IDLE;
          return;
        }
        if (n > RX_MAX) {
          ESP_LOGW(TAG, "RX overflow for %s, dropping", active_.cmd.c_str());
          state_ = STATE_IDLE;
          return;
        }
      }
    }
    if ((int32_t) (millis() - deadline_) > 0) {
      ESP_LOGW(TAG, "Timeout waiting for response to %s", active_.cmd.c_str());
      state_ = STATE_IDLE;
    }
  }

  void handle_response_() {
    switch (active_.kind) {
      case REQ_COUNTERS:
        handle_counters_(rx_buf_);
        break;
      case REQ_FLAGS:
        handle_flags_(rx_buf_);
        break;
      case REQ_TYPE:
        type_received_ = true;
        ESP_LOGI(TAG, "Machine type: %s", rx_buf_.c_str());
        publish_text("machine_type", rx_buf_);
        break;
      case REQ_USER:
        ESP_LOGI(TAG, "Response to %s: %s", active_.cmd.c_str(), rx_buf_.c_str());
        break;
    }
  }

  void handle_counters_(const std::string &result) {
    if (result.size() < 64) {
      ESP_LOGW(TAG, "Unexpected RT:0000 response len=%d", (int) result.size());
      return;
    }

    // Parse all counters available (4 hex chars per field starting at pos=3)
    std::vector<long> current = parse_all_counters_(result);

    publish_number("counter_1", get_counter_n_(current, 1));
    publish_number("counter_2", get_counter_n_(current, 2));
    publish_number("counter_3", get_counter_n_(current, 3));
    publish_number("counter_4", get_counter_n_(current, 4));
    publish_number("counter_5", get_counter_n_(current, 5));
    publish_number("counter_6", get_counter_n_(current, 6));
    publish_number("counter_7", get_counter_n_(current, 7));
    publish_number("counter_8", get_counter_n_(current, 8));
    publish_number("counter_9", get_counter_n_(current, 9));
    publish_number("counter_10", get_counter_n_(current, 10));
    publish_number("counter_11", get_counter_n_(current, 11));
    publish_number("counter_12", get_counter_n_(current, 12));
    publish_number("counter_13", get_counter_n_(current, 13));
    publish_number("counter_14", get_counter_n_(current, 14));
    publish_number("counter_15", get_counter_n_(current, 15));
    publish_number("counter_16", get_counter_n_(current, 16));

    publish_counter_changes_(current);

    for (const auto &m : maintenance_) {
      const long v = get_counter_n_(current, m.counter_n);
      if (v >= 0) m.bs->publish_state(v >= m.threshold);
    }
  }

  void handle_flags_(const std::string &ic) {
    if (ic.size() < 7) {
      ESP_LOGW(TAG, "Unexpected IC response len=%d", (int) ic.size());
      return;
    }
    uint8_t a = static_cast<uint8_t>(strtol(ic.substr(3, 2).c_str(), NULL, 16));
    uint8_t b = static_cast<uint8_t>(strtol(ic.substr(5, 2).c_str(), NULL, 16));

    publish_ic_bits_if_changed_(a, b);

    uint8_t trayBit = (a >> 4) & 1u;
    uint8_t left_readyBit = (a >> 2) & 1u;
    uint8_t tankBit = (b >> 5) & 1u;
    uint8_t right_busyBit = (b >> 6) & 1u;

    std::string tray_status = (trayBit == 1) ? "Present" : "Missing";
    std::string tank_status = (tankBit == 1) ? "Fill Tank" : "OK";
    std::string machine_status = "Ready";
    if (trayBit == 0) machine_status = "Tray Missing";
    if (tankBit == 1) machine_status = "Fill Tank";
    if (right_busyBit == 1) machine_status = "Busy (Milk Drink)";
    if (left_readyBit == 0) machine_status = "Busy (Coffee Drink)";

    publish_text("tray_status", tray_status);
    publish_text("water_tank_status", tank_status);
    publish_text("machine_status", machine_status);
  }

  // Return counter_n (1-based) from vector or -1 if missing
  long get_counter_n_(const std::vector<long> &v, int n) const {
    const size_t idx = (n >= 1) ? (size_t) (n - 1) : (size_t) -1;
    if (idx < v.size()) return v[idx];
    return -1;
  }

  // Parse all 4-hex counters from RT payload starting at pos=3
  std::vector<long> parse_all_counters_(const std::string &rt) const {
    std::vector<long> out;
    // field i starts at pos = 3 + 4*i (i = 0..)
    for (size_t pos = 3; pos + 4 <= rt.size(); pos += 4) {
      long val = strtol(rt.substr(pos, 4).c_str(), nullptr, 16);
      out.push_back(val);
    }
    return out;
  }

  void publish_counter_changes_(const std::vector<long> &current) {
    if (!last_counters_initialized_) {
      last_counters_ = current;
      last_counters_initialized_ = true;
      return;
    }

    std::string msg;
    bool any = false;
    const size_t max_n = std::max(last_counters_.size(), current.size());
    char buf[48];

    for (size_t i = 0; i < max_n; ++i) {
      long prev = (i < last_counters_.size()) ? last_counters_[i] : -1;
      long now = (i < current.size()) ? current[i] : -1;
      if (prev != now) {
        if (any) msg += ", ";
        // "counter_%zu %ld→%ld"
        snprintf(buf, sizeof(buf), "counter_%u %ld→%ld", (unsigned) (i + 1), prev, now);
        msg += buf;
        any = true;
      }
    }

    if (any) {
      publish_text("counters_changed", msg);
      ESP_LOGD(TAG, "Changed: %s", msg.c_str());
    }

    last_counters_ = current;
  }

  static inline void byte_to_bits(uint8_t v, char out[9]) {
    for (int i = 7; i >= 0; --i) out[7 - i] = (v & (1u << i)) ? '1' : '0';
    out[8] = '\0';
  }

  void publish_ic_bits_if_changed_(uint8_t a, uint8_t b) {
    if (!ic_bits_initialized_ || a != last_ic_a_ || b != last_ic_b_) {
      char abits[9], bbits[9], buf[32];
      byte_to_bits(a, abits);
      byte_to_bits(b, bbits);
      // "A=xxxxxxxx B=xxxxxxxx"
      snprintf(buf, sizeof(buf), "A=%s B=%s", abits, bbits);
      publish_text("ic_bits", std::string(buf));

      last_ic_a_ = a;
      last_ic_b_ = b;
      ic_bits_initialized_ = true;

      ESP_LOGD(TAG, "IC bits changed: %s", buf);
    }
  }

  std::string model_{"UNKNOWN"};

  std::map<std::string, std::vector<sensor::Sensor *>> numeric_;
  std::map<std::string, text_sensor::TextSensor *> text_;

  struct MaintenanceSensor {
    binary_sensor::BinarySensor *bs;
    int counter_n;
    long threshold;
  };
  std::vector<MaintenanceSensor> maintenance_;
  bool type_received_{false};

  std::vector<long> last_counters_;
  bool last_counters_initialized_{false};

  uint8_t last_ic_a_{0};
  uint8_t last_ic_b_{0};
  bool ic_bits_initialized_{false};

  // command/response state machine
  State state_{STATE_IDLE};
  std::deque<Request> queue_;
  Request active_{};
  std::string tx_buf_;
  size_t tx_pos_{0};
  uint32_t next_char_at_{0};
  uint32_t deadline_{0};
  std::string rx_buf_;
  uint8_t rx_byte_{0};
  uint8_t rx_bits_{0};
};

}  // namespace jura
}  // namespace esphome
