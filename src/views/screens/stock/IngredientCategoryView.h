/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/food/IngredientCategoryType.h"
#include "core/state/UIState.h"
#include "core/supply/Supplier.h"
#include "views/screens/stock/StockMain.h"

#include <map>

class IngredientCategoryView : public views::screens::stock::StockMain {
public:
  IngredientCategoryView(
      GameContext& context, GameState& state, IngredientCategory category,
      IngredientScreenMode mode = IngredientScreenMode::RESTAURANT);
  void tick() override {};
  void render(const RenderContext& ctx) override;

protected:
  void draw_page_header(const RenderContext& ctx);
  void draw_page_screen(const RenderContext& ctx);
  void draw_depot_overview(const RenderContext& ctx);
  void draw_ingredient_list(const RenderContext& ctx);
  void draw_icon(const RenderContext& ctx);
  void set_up_input_panel() override;

  IngredientCategory category() const { return category_; }
  SupplierType category_to_supplier_type(IngredientCategory cat) const;

  IngredientCategory category_;
  IngredientScreenMode mode_;

private:
  // Pending purchase quantities (keyed by ingredient_id)
  std::map<int, int> pending_quantities_;
  bool show_priorities_ = false;
  int pressed_priority_row_ = -1;

  void draw_priorities(const RenderContext& ctx);
};
