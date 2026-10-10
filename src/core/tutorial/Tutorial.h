/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "core/tutorial/TutorialStep.h"

class GameState;

namespace core::tutorial {

// The tutorial: the step the player is on, and when it moves on.
class Tutorial {
public:
  explicit Tutorial(GameState& state) : gs_(&state) {}

  void start();
  // Ends the tutorial. The game continues as a normal one.
  void finish();

  [[nodiscard]] bool active() const { return step_.has_value(); }
  [[nodiscard]] std::optional<std::size_t> step_index() const { return step_; }
  [[nodiscard]] const Step* current_step() const;
  [[nodiscard]] bool on_last_step() const;
  [[nodiscard]] static std::size_t step_count();

  void advance();
  void item_clicked(std::string_view name);
  void item_hovered(std::string_view name) { hovered_item_ = name; }
  [[nodiscard]] const std::string& hovered_item() const {
    return hovered_item_;
  }
  void tick();

private:
  void enter(std::size_t index);
  void present();

  GameState* gs_;
  std::optional<std::size_t> step_;
  // Counts the steps shown, so a dismissal of a step no longer on screen
  // does nothing.
  uint32_t show_count_{0};
  // Number of ticks in a row the current step's condition has been true.
  int steady_ticks_{0};
  std::string hovered_item_;
};

} // namespace core::tutorial
