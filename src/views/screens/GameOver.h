/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
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
  enum class GameOverScreen { HighScore, NoHighScore };
  GameOver(GameContext& context, GameState& state, GameOverState gos);
  void tick() override {};
  void render(const RenderContext& ctx) override;

private:
  void continue_playing();
  void go_to_main_menu();
  void start_flow();
  void after_high_score();

  Player* player_;
  GameOverCause cause_;
  GameOverScreen screen_;
  bool popup_pending_{true};
};
