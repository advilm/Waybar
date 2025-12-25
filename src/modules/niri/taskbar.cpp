#include "modules/niri/taskbar.hpp"

#include <gtkmm/button.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <spdlog/spdlog.h>

namespace waybar::modules::niri {

Taskbar::Taskbar(const std::string &id, const Bar &bar, const Json::Value &config)
    : AModule(config, "taskbar", id, false, false), bar_(bar), box_(bar.orientation, 0) {
  box_.set_name("taskbar");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);

  if (config_["icon-theme"].isArray()) {
    for (auto &c : config_["icon-theme"]) {
      icon_loader_.add_custom_icon_theme(c.asString());
    }
  } else if (config_["icon-theme"].isString()) {
    icon_loader_.add_custom_icon_theme(config_["icon-theme"].asString());
  }

  if (!gIPC) gIPC = std::make_unique<IPC>();

  gIPC->registerForIPC("WorkspaceActivated", this);
  gIPC->registerForIPC("WindowFocusChanged", this);
  gIPC->registerForIPC("WindowOpenedOrChanged", this);
  gIPC->registerForIPC("WindowClosed", this);
  gIPC->registerForIPC("WindowLayoutsChanged", this);

  dp.emit();
}

Taskbar::~Taskbar() { gIPC->unregisterForIPC(this); }

void Taskbar::onEvent(const Json::Value &ev) { dp.emit(); }

void Taskbar::doUpdate() {
  auto ipcLock = gIPC->lockData();

  const auto &workspaces = gIPC->workspaces();
  auto active_workspace_it = std::find_if(workspaces.cbegin(), workspaces.cend(),
   [&](const auto &ws) {
     return ws["output"].asString() == bar_.output->name && ws["is_active"].asBool();
   });

  if (active_workspace_it == workspaces.cend()) {
    spdlog::warn("No active workspace found for output {}", bar_.output->name);
    box_.hide();
    return;
  }

  const auto &active_workspace = *active_workspace_it;
  const auto active_window_id = active_workspace["active_window_id"].isNull()
                                    ? 0ULL
                                    : active_workspace["active_window_id"].asUInt64();
  const auto &windows = gIPC->windows();
  std::vector<Json::Value> my_windows;
  std::copy_if(windows.cbegin(), windows.cend(), std::back_inserter(my_windows),
               [&](const auto &win) {
                 return win["workspace_id"].asUInt64() == active_workspace["id"].asUInt64();
               });

  std::sort(my_windows.begin(), my_windows.end(), [&](const auto &a, const auto &b) {
    const auto a_layout = a["layout"];
    const auto b_layout = b["layout"];
    const auto a_pos = a_layout["pos_in_scrolling_layout"];
    const auto b_pos = b_layout["pos_in_scrolling_layout"];

    if (a_pos.isNull()) return false;
    if (b_pos.isNull()) return true;

    if (a_pos[0].asUInt() == b_pos[0].asUInt()) {
      return a_pos[1].asUInt() < b_pos[1].asUInt();
    }
    return a_pos[0].asUInt() < b_pos[0].asUInt();
  });

  for (auto &button : buttons_) {
    box_.remove(*button);
  }
  buttons_.clear();

  int icon_size = config_["icon-size"].isInt() ? config_["icon-size"].asInt() : 16;
  for (const auto &win : my_windows) {
    auto button = std::make_unique<Gtk::Button>();
    button->set_relief(Gtk::RELIEF_NONE);

    auto icon = Gtk::make_managed<Gtk::Image>();
    std::string app_id = win["app_id"].asString();

    const auto window_id = win["id"].asUInt64();
    if (config_["on-click"].isString() || config_["on-click-middle"].isString() ||
        config_["on-click-right"].isString()) {
      button->add_events(Gdk::BUTTON_PRESS_MASK);
      button->signal_button_release_event().connect([window_id, this](GdkEventButton* event) {
        spdlog::debug("Focusing window {} {}", window_id, event->button);
        try {
          std::string action_in;
          if (config_["on-click"].isString() && event->button == 1)
            action_in = config_["on-click"].asString();
          else if (config_["on-click-middle"].isString() && event->button == 2)
            action_in = config_["on-click-middle"].asString();
          else if (config_["on-click-right"].isString() && event->button == 3)
            action_in = config_["on-click-right"].asString();

          std::string action_out;
          if (action_in == "activate")
            action_out = "FocusWindow";
          else if (action_in == "maximize")
            action_out = config_["fullscreen-method"].isString()
              ? config_["fullscreen-method"].asString()
              : "MaximizeWindowToEdges";
          else if (action_in == "fullscreen")
            action_out = "FullscreenWindow";
          else if (action_in == "close")
            action_out = "CloseWindow";
          else {
            spdlog::warn("Unknown action {}", action_in);
            return true;
          };


          Json::Value request(Json::objectValue);
          auto &action = (request["Action"] = Json::Value(Json::objectValue));
          auto &windowAction = (action[action_out] = Json::Value(Json::objectValue));
          windowAction["id"] = window_id;
          IPC::send(request);

          return true;
        } catch (const std::exception &e) {
          spdlog::error("Error focusing window: {}", e.what());

          return false;
        }
      });
    }

    if (!app_id.empty()) {
      auto app_info = IconLoader::get_app_info_from_app_id_list(app_id);
      if (icon_loader_.image_load_icon(*icon, app_info, icon_size)) {
        icon->show();
      }
    }

    button->add(*icon);

    auto style_context = button->get_style_context();
    if (window_id == active_window_id) {
      style_context->add_class("active");
    } else {
      style_context->remove_class("active");
    }

    button->show();
    box_.pack_start(*button, false, false);
    buttons_.push_back(std::move(button));
  }

  if (my_windows.empty()) {
    box_.get_style_context()->add_class("empty");
  } else {
    box_.get_style_context()->remove_class("empty");
  }

  box_.show();
}

void Taskbar::update() {
  doUpdate();
  AModule::update();
}


} /* namespace waybar::modules::niri */
