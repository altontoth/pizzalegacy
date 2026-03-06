/*
 * SPDX-FileCopyrightText: 2016 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "views/base/MainPanelView.h"
#include "views/ui/status/StatusController.h"

class PurchasingMain : public MainPanelView {
public:
  PurchasingMain(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;
  virtual void set_up_input_panel();
  StatusController status_controller_;
};
