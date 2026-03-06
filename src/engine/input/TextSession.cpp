/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/input/TextSession.h"

#include "engine/input/InputManager.h"

namespace input {

void TextSession::release() const {
  if (mgr_) {
    mgr_->end_text_input();
  }
}

void TextSession::end_if_active() noexcept {
  if (!mgr_) {
    return;
  }
  mgr_->end_text_input();
  mgr_ = nullptr;
}

void TextSession::steal(TextSession& other) noexcept {
  mgr_ = other.mgr_;
  other.mgr_ = nullptr;
}

} // namespace input
