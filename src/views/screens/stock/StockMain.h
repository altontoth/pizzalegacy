/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/state/InterruptionRate.h"
#include "views/base/RestaurantSubView.h"
#include "views/ui/status/StatusController.h"

namespace views::screens::stock {

class StockMain : public RestaurantSubView {
public:
  StockMain(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;
  InterruptionRate interruption_rate() const override {
    return InterruptionRate::OnAction;
  }
  virtual void set_up_input_panel();
  void navigate_to_restaurant(core::RestaurantId new_id) override;
  StatusController status_controller_;
};

} // namespace views::screens::stock
