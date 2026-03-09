/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/state/UIState.h"
#include "core/util/GameContext.h"
#include "views/base/View.h"

class Player;

class GameOver : public View {
public:
  GameOver(GameContext& context, GameState& state, GameOverState gos);
  void tick() override {};
  void render(const RenderContext& ctx) override;

private:
  const Player* player_;
};
