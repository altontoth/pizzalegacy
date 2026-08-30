/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/food/Ingredient.h"
#include "core/food/IngredientCategoryType.h"
#include "views/screens/stock/StockMain.h"

#include <vector>

class City;

namespace core::city {
class CentralDepot;
}

struct ExportItem {
  int ingredient_id;
  const core::food::Ingredient* ingredient;
  int source_stock_kg;
  int target_stock_kg;
  int transfer_amount{0};
};

class DepotExportScreen : public views::screens::stock::StockMain {
public:
  DepotExportScreen(GameContext& context, GameState& state,
                    IngredientCategory category, core::CityId target_city_id,
                    int transport_cost);
  void tick() override;
  void render(const RenderContext& ctx) override;

protected:
  void set_up_input_panel() override;

private:
  void draw_page_header(const RenderContext& ctx);
  void draw_ingredient_list(const RenderContext& ctx);
  void execute_export();
  void charge_exit();

  void handle_button_press(size_t ingredient_index, bool is_left_button);
  void handle_button_release(bool is_left_button);
  void update_held_button_repeat();

  IngredientCategory category_;
  core::CityId target_city_id_;
  int transport_cost_;
  City* source_city_;
  City* target_city_;
  std::vector<ExportItem> items_;

  int hovered_index_{-1};
  int button_held_index_{-1};
  bool left_button_held_{false};
  bool right_button_held_{false};
  bool increment_direction_{true};
  int button_held_frames_{0};
  static constexpr int REPEAT_DELAY = 6;
  static constexpr int REPEAT_RATE_SINGLE = 2;
  static constexpr int REPEAT_RATE_DUAL = 1;
};
