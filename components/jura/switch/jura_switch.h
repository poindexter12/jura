#pragma once

#include "esphome/components/switch/switch.h"
#include "../jura.h"

namespace esphome {
namespace jura {

class JuraPowerSwitch : public switch_::Switch {
 public:
  void set_parent(Jura *parent) { parent_ = parent; }

 protected:
  // Optimistic: the machine offers no queryable power state, so the switch
  // reflects the last command sent. AN:01 works from ready/standby; whether
  // it can wake a fully sleeping machine is hardware-dependent (see the
  // README hardware section).
  void write_state(bool state) override {
    if (parent_ != nullptr) parent_->send_command(state ? "AN:01" : "AN:02");
    this->publish_state(state);
  }

  Jura *parent_{nullptr};
};

}  // namespace jura
}  // namespace esphome
