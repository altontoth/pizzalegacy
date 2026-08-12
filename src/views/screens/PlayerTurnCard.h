/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include "core/state/UIState.h"
#include "views/base/MainPanelView.h"

class PlayerTurnCard : public MainPanelView {
public:
  PlayerTurnCard(GameContext& context, GameState& state,
                 PlayerTurnCardState ps);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  core::PlayerId player_;
  int delay_ticks_{2};
  GameDate date_;
  std::function<void()> on_closed_;
};
