/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/util/GameContext.h"
#include "views/base/OverlayBehavior.h"
#include "views/base/View.h"

class GameState;

// Blocks pointer interaction and shows a busy cursor while the end-of-turn
// pipeline runs without waiting for player input. Popups the pipeline shows
// rank above the lock and stay clickable.
class TurnLockLayer : public View, public OverlayBehavior<TurnLockLayer> {
public:
  TurnLockLayer(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  bool locked_{false};
};
