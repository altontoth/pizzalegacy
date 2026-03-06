/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "views/base/MainPanelView.h"
#include "views/ui/status/StatusController.h"

namespace views::screens::depot {

class DepotMain : public MainPanelView {
public:
  DepotMain(GameContext& context, GameState& state, DepotMainState dms = {});
  void tick() override;
  void render(const RenderContext& ctx) override;
  virtual void set_up_input_panel();
  StatusController status_controller_;

private:
  Property* property_{nullptr};
};

} // namespace views::screens::depot
