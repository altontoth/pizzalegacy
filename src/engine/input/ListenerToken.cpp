/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/input/ListenerToken.h"
#include "engine/input/InputManager.h"

namespace input {

void ListenerToken::reset() {
  if (mgr && who) {
    mgr->unregister_listener(*who);
    mgr = nullptr;
    who = nullptr;
  }
}

} // namespace input
