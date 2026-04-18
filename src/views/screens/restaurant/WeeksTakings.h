/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include "core/common/Ids.h"
#include "core/restaurant/Restaurant.h"
#include "views/base/MainPanelView.h"

class Restaurant;

class WeeksTakings : public MainPanelView {
public:
  WeeksTakings(GameContext& context, GameState& state,
               core::RestaurantId restaurant_id, bool end_of_week_mode,
               std::function<void()> on_done = {});
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  struct WeeklyTakings {
    std::vector<std::pair<core::RecipeId, core::restaurant::PizzaSalesData>>
        pizza_sales;
    Money total_revenue{0};
    Money total_costs{0};
    Money total_profit{0};
    std::array<Money, 4> ingredient_purchases_by_category{Money{0}, Money{0},
                                                          Money{0}, Money{0}};
    Money total_staff_wages{0};
  };

  void create_buttons();
  void render_background(const RenderContext& ctx);
  WeeklyTakings aggregate_weekly_takings() const;
  void render_pizza_sales_page(const RenderContext& ctx,
                               const WeeklyTakings& weekly, int page);
  void render_expenses_page(const RenderContext& ctx,
                            const WeeklyTakings& weekly);
  void dismiss();

  core::RestaurantId restaurant_id_;
  Restaurant* restaurant_{nullptr};
  int current_page_{0};
  bool end_of_week_mode_{false};
  std::function<void()> on_done_;
};
