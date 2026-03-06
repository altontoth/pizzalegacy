/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/food/IngredientCategoryType.h"
#include "core/state/UIState.h"
#include "views/helpers/SupplierListRenderer.h"
#include "views/screens/stock/StockMain.h"

#include <map>
#include <optional>
#include <string>

class FoodSupplier;

class IngredientSupplierList : public views::screens::stock::StockMain {
public:
  IngredientSupplierList(
      GameContext& context, GameState& state, IngredientCategory category,
      IngredientScreenMode mode = IngredientScreenMode::RESTAURANT);
  void tick() override;
  void render(const RenderContext& ctx) override;

protected:
  void draw_page_header(const RenderContext& ctx);
  void draw_icon(const RenderContext& ctx);
  void draw_page_screen(const RenderContext& ctx);
  void set_up_input_panel() override;

private:
  IngredientCategory category_;
  IngredientScreenMode mode_;
  std::map<IngredientCategory, std::string> category_titles_;
  std::optional<SupplierListRenderer<FoodSupplier>> renderer_;
  std::size_t current_page_{0};
};
