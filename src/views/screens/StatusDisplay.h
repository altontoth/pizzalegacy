/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/status/StatusHub.h"
#include "views/base/PanelView.h"

class StatusDisplay : public PanelView {
public:
  StatusDisplay(GameContext& context, GameState& state, StatusHub& hub);
  void tick() override;
  void render(const RenderContext& ctx) override;

  // Freeze/unfreeze header display (date, time, money)
  void freeze();
  void unfreeze();

private:
  void clear(const RenderContext& ctx);
  void render_header_info(const RenderContext& ctx);
  void recipe(StatusDisplayRecipe r);
  void render_recipe(const StatusDisplayRecipe& recipe,
                     const RenderContext& ctx);

  StatusDisplayRecipe recipe_;
  int previous_time_{-1};
  StatusHub& hub_;
  StatusHub::Subscription sub_;

  // Frozen header state
  bool frozen_{false};
  std::string frozen_date_;
  std::string frozen_time_;
  std::string frozen_money_;
};
