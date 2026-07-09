/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
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
  void offer_save_career();
  void save_career_as_mission(const std::string& raw_name);
  void finish_flow();

  Player* player_;
  GameOverCause cause_;
  GameOverScreen screen_;
  bool popup_pending_{true};
  // Overall mission match percentage, set for GameOverCause::MissionComplete.
  std::optional<int> mission_score_;
};
