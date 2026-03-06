/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"

class GlobalActionHandler : public input::InputListener,
                            public debug::AutoLabel<GlobalActionHandler> {
public:
  struct Callbacks {
    std::function<void()> request_exit;
    std::function<void()> request_screenshot;
    std::function<void()> toggle_fullscreen;
  };

  GlobalActionHandler(input::InputManager& in, Callbacks cb);
  bool on_action(const input::ActionEvent& ev) override;

private:
  input::ListenerToken listener_;
  Callbacks callbacks_;
  bool fullscreen_{false};
};
