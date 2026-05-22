/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <vector>
#include "core/common/Ids.h"
#include "core/food/IngredientArray.h"
#include "core/food/IngredientCategoryType.h"
#include "core/food/IngredientType.h"
#include "core/logistics/IngredientStock.h"

namespace core::city {

using core::restaurant::IngredientStock;

constexpr int max_delivery_priority_slots = 12;

class CentralDepot {
public:
  CentralDepot() = default;
  int capacity() const;
  int used() const;

  void add_capacity(int delta);
  void remove_capacity(int delta);

  void steal_stock(IngredientCategory category);

  IngredientStock& get_stock(core::food::IngredientType ingredient);
  void add_ingredient(core::food::IngredientType ingredient, int qty_grams,
                      int quality, Money cost_per_kg);
  bool consume_ingredient(core::food::IngredientType ingredient, int qty_grams);
  const core::food::IngredientArray<IngredientStock>& all_stocks() const;
  core::food::IngredientArray<IngredientStock>& all_stocks();

  const IngredientStock* get_ingredient_stock(int ingredient_id) const;
  IngredientStock* get_ingredient_stock(int ingredient_id);

  // Delivery priority: ordered list of restaurant IDs.
  // The first max_delivery_priority_slots entries can be reordered via the UI.
  // Restaurants beyond slot 12 are delivered to in their default order.
  const std::vector<core::RestaurantId>& delivery_priorities() const;

  // Initialize priorities from a list of restaurants (keeps existing order
  // for restaurants already in the list, appends new ones).
  void sync_delivery_priorities(
      const std::vector<core::RestaurantId>& player_restaurants);

  // Move a restaurant to the top of the priority list
  void promote_to_top(core::RestaurantId id);
  void delivery_priorities(std::vector<core::RestaurantId> priorities);
  std::optional<IngredientCategory> take_stock_stolen();
  std::optional<IngredientCategory> stock_stolen() const {
    return stock_stolen_;
  }
  void stock_stolen(std::optional<IngredientCategory> val) {
    stock_stolen_ = val;
  }

private:
  int capacity_ = 0;
  core::food::IngredientArray<IngredientStock> stocks_;
  std::vector<core::RestaurantId> delivery_priorities_;
  std::optional<IngredientCategory> stock_stolen_;
};

} // namespace core::city
