/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <vector>
#include "core/common/Ids.h"
#include "core/food/IngredientArray.h"
#include "core/logistics/IngredientStock.h"

namespace core::city {

using core::restaurant::IngredientStock;

constexpr int max_delivery_priority_slots = 12;

class CentralDepot {
public:
  CentralDepot() = default;
  int capacity() const { return capacity_; }
  int used() const { return used_; }

  void add_capacity(int delta) { capacity_ += delta; }
  void remove_capacity(int delta) { capacity_ -= delta; }

  IngredientStock& get_stock(core::food::IngredientType ingredient) {
    return stocks_[ingredient];
  }
  void add_ingredient(core::food::IngredientType ingredient, int qty_grams,
                      int quality, Money cost_per_kg) {
    get_stock(ingredient).add_stock(qty_grams, quality, cost_per_kg);
  }
  bool consume_ingredient(core::food::IngredientType ingredient,
                          int qty_grams) {
    return get_stock(ingredient).consume(qty_grams);
  }
  const core::food::IngredientArray<IngredientStock>& all_stocks() const {
    return stocks_;
  }
  core::food::IngredientArray<IngredientStock>& all_stocks() { return stocks_; }

  const IngredientStock* get_ingredient_stock(int ingredient_id) const {
    if (ingredient_id < 0 || ingredient_id >= max_ingredient_count) {
      return nullptr;
    }
    return &stocks_[ingredient_id];
  }

  IngredientStock* get_ingredient_stock(int ingredient_id) {
    if (ingredient_id < 0 || ingredient_id >= max_ingredient_count) {
      return nullptr;
    }
    return &stocks_[ingredient_id];
  }

  // Delivery priority: ordered list of restaurant IDs.
  // The first max_delivery_priority_slots entries can be reordered via the UI.
  // Restaurants beyond slot 12 are delivered to in their default order.
  // (No pagination in the original so we just match that)
  const std::vector<core::RestaurantId>& delivery_priorities() const {
    return delivery_priorities_;
  }

  // Initialize priorities from a list of restaurants (keeps existing order
  // for restaurants already in the list, appends new ones).
  void sync_delivery_priorities(
      const std::vector<core::RestaurantId>& player_restaurants) {
    // Remove any IDs that no longer exist
    std::erase_if(delivery_priorities_, [&](core::RestaurantId id) {
      return std::find(player_restaurants.begin(), player_restaurants.end(),
                       id) == player_restaurants.end();
    });
    // Append any new restaurants not yet in the list
    for (auto id : player_restaurants) {
      if (std::ranges::find(delivery_priorities_, id) ==
          delivery_priorities_.end()) {
        delivery_priorities_.push_back(id);
      }
    }
  }

  // Move a restaurant to the top of the priority list
  void promote_to_top(core::RestaurantId id) {
    auto it =
        std::find(delivery_priorities_.begin(), delivery_priorities_.end(), id);
    if (it != delivery_priorities_.end() &&
        it != delivery_priorities_.begin()) {
      delivery_priorities_.erase(it);
      delivery_priorities_.insert(delivery_priorities_.begin(), id);
    }
  }

private:
  int capacity_ = 0;
  int used_ = 0;
  core::food::IngredientArray<IngredientStock> stocks_;
  std::vector<core::RestaurantId> delivery_priorities_;
};

} // namespace core::city
