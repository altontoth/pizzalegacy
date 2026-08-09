/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>

#include "core/util/GameContext.h"
#include "views/base/OverlayBehavior.h"
#include "views/base/View.h"

class GameState;

// Texts to tell the user what the game is doing (saving, etc)
class GameInfoLayer : public View, public OverlayBehavior<GameInfoLayer> {
public:
  GameInfoLayer(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool needs_force_recompose() const override { return pull_recompose_flag(); }
  // Captions stay readable during screen fades.
  bool ignore_fade() const override { return true; }

private:
  std::string activity_;
  std::string time_scale_;
};
