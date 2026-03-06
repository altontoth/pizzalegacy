/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <utility>

namespace input {

class InputManager;
class InputListener;

// RAII token for keyboard input
struct ListenerToken {
  InputManager* mgr{nullptr};
  InputListener* who{nullptr};
  ListenerToken() = default;
  ListenerToken(InputManager& m, InputListener& w) : mgr(&m), who(&w) {}
  ListenerToken(const ListenerToken&) = delete;
  ListenerToken& operator=(const ListenerToken&) = delete;
  ListenerToken(ListenerToken&& o) noexcept { *this = std::move(o); }
  ListenerToken& operator=(ListenerToken&& o) noexcept {
    if (this != &o) {
      reset();
      mgr = o.mgr;
      who = o.who;
      o.mgr = nullptr;
      o.who = nullptr;
    }
    return *this;
  }
  ~ListenerToken() { reset(); }
  void reset();
};

} // namespace input
