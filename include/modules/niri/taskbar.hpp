#pragma once

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/niri/backend.hpp"
#include "util/icon_loader.hpp"

#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <memory>
#include <vector>

namespace waybar::modules::niri {

class Taskbar : public AModule, public EventHandler {
 public:
  Taskbar(const std::string &, const Bar &, const Json::Value &);
  ~Taskbar() override;
  void update() override;

 private:
  void onEvent(const Json::Value &ev) override;
  void doUpdate();

  const Bar &bar_;
  Gtk::Box box_;
  IconLoader icon_loader_;
  std::vector<std::unique_ptr<Gtk::Button>> buttons_;
};

}  // namespace waybar::modules::niri
