/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/food/IngredientCategoryType.h"
#include "views/base/MainPanelView.h"
#include "views/ui/status/StatusController.h"

#include <optional>

class DepotIngredientCategoryView : public MainPanelView {
public:
  DepotIngredientCategoryView(GameContext& context, GameState& state,
                              IngredientCategory category);
  void tick() override {}
  void render(const RenderContext& ctx) override;

private:
  void draw_page_header(const RenderContext& ctx);
  void draw_icon(const RenderContext& ctx);
  void draw_ingredient_list(const RenderContext& ctx);
  void draw_priorities(const RenderContext& ctx);
  void set_up_input_panel();

  StatusController status_controller_;
  IngredientCategory category_;
  bool show_priorities_ = false;
  std::optional<int> pressed_priority_row_;
};
