/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "views/screens/InputPanel.h"

#include <memory>

// This class is the parent of various main panel views, e.g. RestaurantView,
// PizzaDesign, etc
class MainPanelView : public PanelView {
public:
  MainPanelView(GameContext& context, GameState& state);
  void tick() override = 0;
  void render(const RenderContext& ctx) override = 0;
  InputPanel* input_panel();

protected:
  std::unique_ptr<InputPanel> input_panel_;

private:
  Surface input_surface_;
};
