/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/DebugLabel.h"
#include "views/base/MainPanelView.h"
#include "views/base/PanelContainerView.h"
#include "views/base/SharedPanel.h"
#include "views/base/View.h"

class PaneledView : public View, public debug::AutoLabel<PaneledView> {
public:
  PaneledView(GameContext& context, GameState& state,
              std::unique_ptr<View> inner);
  void tick() override;
  void render(const RenderContext& ctx) override;
  InterruptionRate interruption_rate() const override {
    return main_panel_view_->interruption_rate();
  }
  ClockRate clock_rate() const override {
    return main_panel_view_->clock_rate();
  }
  void status_panel(SharedPanel*);
  void input_panel(SharedPanel*);
  std::vector<LayerInfo> get_layers() override;

protected:
  // main_panel is different because it doesn't need access to itself via a
  // pointer.
  std::unique_ptr<SharedPanel> input_panel_;
  std::unique_ptr<SharedPanel> status_panel_;

  Surface main_surface_;
  std::unique_ptr<MainPanelView> main_panel_view_;

private:
  void view_render(const RenderContext& ctx, View*, ViewRect);
  std::unique_ptr<PanelContainerView> panel_container_view_;
};
