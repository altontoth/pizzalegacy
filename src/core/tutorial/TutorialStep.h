/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/city/City.h"
#include "core/state/UIState.h"
#include "engine/FormatCompat.h"

class GameState;

namespace core::tutorial {

// Whether the current screen is the one we need for our step.
using ScreenMatch = bool (*)(const UIState& top);

template <class S> bool on_screen(const UIState& top) {
  return std::holds_alternative<S>(top);
}

struct MapTile {
  int x = 0;
  int y = 0;

  [[nodiscard]] int index() const { return City::x_y_to_index(x, y); }
  [[nodiscard]] std::string item_name() const {
    return compat::format("property_{}", index());
  }
};

constexpr std::string_view skip_item = "tutorial_skip";

struct ActivePopup {};

// What the player has to interact with or look at (so the tutorial can
// highlight it).
using Target = std::variant<std::string, MapTile, ActivePopup>;

// While active, only these items can be clicked: the target, the tutorial's
// own buttons and the ones listed here.
struct ClickFilter {
  bool active = false;
  // Items whose name starts with one of these can be clicked as well.
  std::vector<std::string> allowed_prefixes;

  static ClickFilter target_only() { return {.active = true}; }
  static ClickFilter allow(std::vector<std::string> prefixes) {
    return {.active = true, .allowed_prefixes = std::move(prefixes)};
  }
};

// The position of the tutorial box. Auto keeps it out of the way of the Target.
enum class PanelPlace { Auto, Top, Bottom };

enum class Completion {
  Dismissed,
  TargetClicked,
  Condition,
};

struct Step {
  // For debugging and the console.
  std::string name;
  std::string text;
  // If a Target is present this specifies on which screen that target can be
  // found.
  ScreenMatch screen = nullptr;
  std::optional<Target> target;
  ClickFilter click_filter;
  PanelPlace panel = PanelPlace::Auto;
  Completion completion = Completion::Dismissed;
  std::function<bool(const GameState&)> condition;
  // Number of ticks in a row the condition must be true before the step is
  // done.
  int steady_ticks = 0;
  std::function<bool(const GameState&)> skip_if;
  std::function<void(GameState&)> on_enter;

  [[nodiscard]] bool on_its_screen(const UIState& top) const {
    return !screen || screen(top);
  }
};

} // namespace core::tutorial
