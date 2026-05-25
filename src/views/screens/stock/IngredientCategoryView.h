/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/food/IngredientCategoryType.h"
#include "views/screens/stock/StockMain.h"

#include <map>

class IngredientCategoryView : public views::screens::stock::StockMain {
public:
  IngredientCategoryView(GameContext& context, GameState& state,
                         IngredientCategory category);
  void tick() override {};
  void render(const RenderContext& ctx) override;

protected:
  void draw_page_header(const RenderContext& ctx);
  void draw_ingredient_list(const RenderContext& ctx);
  void draw_icon(const RenderContext& ctx);
  void set_up_input_panel() override;
  void navigate_to_restaurant(core::RestaurantId new_id) override;

  IngredientCategory category_;

private:
  std::map<int, int> pending_quantities_;
};
