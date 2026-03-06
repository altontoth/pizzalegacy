/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "views/base/MainPanelView.h"

class Restaurant;

class StatsRestaurants : public MainPanelView {
public:
  StatsRestaurants(GameContext& context, GameState& state,
                   core::PlayerId player_id);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  core::PlayerId player_id_;
  void create_buttons();
  int current_page_{0};
  int max_pages_{0};
  const Bob& stanbobs_;
  const Surface& tiles_medium_;
  void draw_restaurant_overview(const RenderContext& ctx, ViewPoint pos,
                                Restaurant* r);
};
