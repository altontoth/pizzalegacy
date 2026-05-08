/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/state/UIState.h"
#include "core/util/GameContext.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "views/base/View.h"

class Player;

class GameOver : public View, public input::InputListener {
public:
  GameOver(GameContext& context, GameState& state, GameOverState gos);
  void tick() override {}
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  enum class Phase { ContinuePrompt, ScoreScreen };

  void continue_playing();
  void go_to_main_menu();

  Player* player_;
  GameOverCause cause_;
  Phase phase_;
  input::ListenerToken listener_;
};
