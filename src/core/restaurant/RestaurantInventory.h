/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/food/Ingredient.h"
#include "core/food/IngredientArray.h"
#include "core/logistics/IngredientStock.h"

namespace core::restaurant {

class RestaurantInventory {
public:
  RestaurantInventory() = default;

  IngredientStock& get_stock(core::food::IngredientType ingredient) {
    return stocks_[ingredient];
  }

  const IngredientStock&
  get_stock(core::food::IngredientType ingredient) const {
    return stocks_[ingredient];
  }

  void add_ingredient(core::food::IngredientType ingredient, int qty_grams,
                      int quality, Money cost_per_kg) {
    get_stock(ingredient).add_stock(qty_grams, quality, cost_per_kg);

    if (average_stock_quality_ == 0) {
      average_stock_quality_ = static_cast<uint8_t>(quality);
    }
    else {
      average_stock_quality_ =
          static_cast<uint8_t>((average_stock_quality_ + quality) / 2);
    }
  }

  bool consume_ingredient(core::food::IngredientType ingredient,
                          int qty_grams) {
    return get_stock(ingredient).consume(qty_grams);
  }

  bool can_fulfill_recipe(
      const std::vector<std::pair<core::food::IngredientType, int>>& recipe)
      const {
    for (const auto& [ingredient, qty_grams] : recipe) {
      if (!get_stock(ingredient).has_available(qty_grams)) {
        return false;
      }
    }
    return true;
  }

  bool consume_recipe(
      const std::vector<std::pair<core::food::IngredientType, int>>& recipe) {
    // First check if all ingredients are available
    if (!can_fulfill_recipe(recipe)) {
      return false;
    }

    // Then consume all
    for (const auto& [ingredient, qty_grams] : recipe) {
      consume_ingredient(ingredient, qty_grams);
    }
    return true;
  }

  void apply_daily_degradation(float degradation_rate = 0.95f) {
    for (auto& stock : stocks_) {
      if (stock.quantity > 0) {
        stock.apply_degradation(degradation_rate);
      }
    }
  }

  Money total_value() const {
    Money total{0};
    for (const auto& stock : stocks_) {
      total = total + stock.total_value();
    }
    return total;
  }

  uint8_t average_stock_quality() const { return average_stock_quality_; }

  const core::food::IngredientArray<IngredientStock>& all_stocks() const {
    return stocks_;
  }
  core::food::IngredientArray<IngredientStock>& all_stocks() { return stocks_; }

  const IngredientStock*
  get_ingredient_stock(food::IngredientType ingredient) const {
    return &stocks_[ingredient];
  }

  IngredientStock* get_ingredient_stock(food::IngredientType ingredient) {
    return &stocks_[ingredient];
  }

  std::vector<std::pair<int, const IngredientStock&>> ingredients() const {
    std::vector<std::pair<int, const IngredientStock&>> result;
    for (size_t i = 0; i < stocks_.size(); i++) {
      result.emplace_back(i, stocks_[i]);
    }
    return result;
  }

private:
  core::food::IngredientArray<IngredientStock> stocks_;

  // Running average stock quality (assembly offset +37Fh)
  // Updated on each delivery: new_avg = (old_avg + delivery_quality) / 2
  uint8_t average_stock_quality_{0};
};

} // namespace core::restaurant
