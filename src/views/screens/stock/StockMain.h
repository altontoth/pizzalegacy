/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "views/base/MainPanelView.h"
#include "views/ui/status/StatusController.h"

namespace views::screens::stock {

class StockMain : public MainPanelView {
public:
  StockMain(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;
  virtual void set_up_input_panel();
  StatusController status_controller_;
};

} // namespace views::screens::stock
