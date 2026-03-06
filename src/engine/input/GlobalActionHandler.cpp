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

  default:
    return false; // not a global hotkey
  }
}
