/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>

#include "engine/ui/UiMode.h"

class GameContext;
class GameState;
class View;

// Builds the View for the UIState on top of the UI stack. This is the single
// place that maps UIState variants to concrete screens, so the engine loop
// does not depend on any screen type.
class ViewFactory {
public:
  struct Result {
    // Null when the top state maps to no screen (monostate, or a
    // data-bearing state whose lookups failed).
    std::unique_ptr<View> view;
    UiMode ui_mode{UiMode::Game};
  };

  static Result create(GameContext& context, GameState& gs);
};
