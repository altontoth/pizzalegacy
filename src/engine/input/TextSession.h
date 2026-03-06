/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

namespace input {

class InputManager;

class TextSession {
public:
  TextSession() = default;
  ~TextSession() { release(); }

  TextSession(const TextSession&) = delete;
  TextSession& operator=(const TextSession&) = delete;
  TextSession(TextSession&& other) noexcept { steal(other); }
  TextSession& operator=(TextSession&& other) noexcept {
    if (this != &other) {
      end_if_active();
      steal(other);
    }
    return *this;
  }

private:
  friend class InputManager;
  explicit TextSession(InputManager* mgr) : mgr_(mgr) {}
  void release() const;
  void end_if_active() noexcept;
  void steal(TextSession& other) noexcept;

  InputManager* mgr_ = nullptr;
};

} // namespace input
