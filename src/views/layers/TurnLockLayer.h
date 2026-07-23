/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string_view>

#include "core/util/GameContext.h"
#include "views/base/OverlayBehavior.h"
#include "views/base/View.h"

class GameState;

// Blocks pointer interaction, shows a busy cursor and an activity caption
// while the end-of-turn pipeline runs without waiting for player input.
// Popups the pipeline shows register their interaction items later and stay
// clickable.
class TurnLockLayer : public View, public OverlayBehavior<TurnLockLayer> {
public:
  TurnLockLayer(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool needs_force_recompose() const override { return pull_recompose_flag(); }
  // The caption stays readable while the day transition fades the screen.
  bool ignore_fade() const override { return true; }

private:
  bool locked_{false};
  // Caption drawn in the corner; empty while unlocked.
  std::string_view activity_;
};
