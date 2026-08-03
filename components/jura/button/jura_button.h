#pragma once

#include "esphome/components/button/button.h"
#include "../jura.h"

namespace esphome {
namespace jura {

class JuraButton : public button::Button {
 public:
  void set_parent(Jura *parent) { parent_ = parent; }
  void set_command(const std::string &cmd) { command_ = cmd; }

 protected:
  void press_action() override {
    if (parent_ != nullptr) parent_->send_command(command_);
  }

  Jura *parent_{nullptr};
  std::string command_;
};

}  // namespace jura
}  // namespace esphome
