/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/input/GlobalActionHandler.h"
#include "engine/Logger.h"
#include "engine/input/InputManager.h"
#include "magic_enum/magic_enum.hpp"

GlobalActionHandler::GlobalActionHandler(input::InputManager& in, Callbacks cb)
    : listener_(in.register_listener(*this)), callbacks_(std::move(cb)) {
  Logger::debug("Registering global listener");
}

static int time_scale_for(input::Action a) {
  switch (a) {
  case input::Action::TimeScale2:
    return 2;
  case input::Action::TimeScale3:
    return 3;
  case input::Action::TimeScale5:
    return 5;
  case input::Action::TimeScale10:
    return 10;
  default:
    return 1;
  }
}

bool GlobalActionHandler::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }

  switch (ev.action) {
  case input::Action::Exit:
    if (callbacks_.request_exit) {
      callbacks_.request_exit();
    }
    return true;

  case input::Action::TakeScreenshot:
    if (callbacks_.request_screenshot) {
      callbacks_.request_screenshot();
    }
    return true;

  case input::Action::ToggleFullscreen:
    fullscreen_ = !fullscreen_;
    if (callbacks_.toggle_fullscreen) {
      callbacks_.toggle_fullscreen();
    }
    return true;

  case input::Action::TimeScale1:
  case input::Action::TimeScale2:
  case input::Action::TimeScale3:
  case input::Action::TimeScale5:
  case input::Action::TimeScale10: {
    if (callbacks_.set_time_scale) {
      callbacks_.set_time_scale(time_scale_for(ev.action));
    }
    return true;
  }

  default:
    return false; // not a global hotkey
  }
}
