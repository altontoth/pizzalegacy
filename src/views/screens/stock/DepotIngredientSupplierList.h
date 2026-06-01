/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/food/IngredientCategoryType.h"
#include "core/state/UIState.h"
#include "views/base/MainPanelView.h"
#include "views/helpers/SupplierListRenderer.h"
#include "views/ui/status/StatusController.h"

#include <map>
#include <optional>
#include <string>

class FoodSupplier;

class DepotIngredientSupplierList : public MainPanelView {
public:
  DepotIngredientSupplierList(GameContext& context, GameState& state,
                              IngredientCategory category,
                              IngredientScreenMode mode);
  void tick() override {}
  void render(const RenderContext& ctx) override;

private:
  void draw_page_header(const RenderContext& ctx);
  void draw_icon(const RenderContext& ctx);
  void draw_page_screen(const RenderContext& ctx);
  void set_up_input_panel();

  StatusController status_controller_;
  IngredientCategory category_;
  IngredientScreenMode mode_;
  std::map<IngredientCategory, std::string> category_titles_;
  std::optional<SupplierListRenderer<FoodSupplier>> renderer_;
  std::size_t current_page_{0};
};
