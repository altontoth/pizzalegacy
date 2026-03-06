/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

namespace input {
struct TextEvent;
struct ActionEvent;

class InputListener {
public:
  virtual ~InputListener() = default;
  virtual bool on_text(const input::TextEvent& ev) { return false; };
  virtual bool on_action(const input::ActionEvent& ev) = 0;
};

} // namespace input
