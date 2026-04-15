/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/PizzaSalesAggregator.h"

#include "core/restaurant/Restaurant.h"
#include "core/time/GameDate.h"

namespace core::restaurant {

PizzaSalesVec aggregate_weekly_pizza_sales(const Restaurant& restaurant,
                                           const GameDate& date) {
  auto week_data = restaurant.stats().current_week(date);

  std::unordered_map<core::RecipeId, PizzaSalesData, core::RecipeId::Hasher>
      aggregated;

  for (const auto& day : week_data) {
    for (const auto& [recipe_id, sales] : day.pizza_sales) {
      auto& agg = aggregated[recipe_id];
      agg.count_ordered += sales.count_ordered;
      agg.count_sold += sales.count_sold;
      agg.revenue += sales.revenue;
      agg.ingredient_cost += sales.ingredient_cost;
      agg.profit += sales.profit;
    }
  }

  PizzaSalesVec result;
  for (const auto& entry : restaurant.menu().entries()) {
    auto it = aggregated.find(entry.recipe_id);
    if (it != aggregated.end()) {
      result.emplace_back(entry.recipe_id, it->second);
    }
    else {
      result.emplace_back(entry.recipe_id, PizzaSalesData{});
    }
  }

  return result;
}

} // namespace core::restaurant
