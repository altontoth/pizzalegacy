/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/CentralDepot.h"

#include <algorithm>
#include "core/food/IngredientCategoryType.h"
#include "core/food/IngredientType.h"

using namespace core::city;

using core::restaurant::IngredientStock;

int CentralDepot::capacity() const { return capacity_; }
int CentralDepot::used() const {
  int total = 0;
  for (const auto& stock : stocks_) {
    total += stock.quantity / 1000;
  }
  return total;
}

void CentralDepot::add_capacity(int delta) { capacity_ += delta; }
void CentralDepot::remove_capacity(int delta) { capacity_ -= delta; }

void CentralDepot::steal_stock(IngredientCategory category) {
  stock_stolen_ = category;
  auto range = core::food::ingredient_range_for_category(category);
  stocks_.fill_range(range.first, range.last, IngredientStock{});
}

IngredientStock&
CentralDepot::get_stock(core::food::IngredientType ingredient) {
  return stocks_[ingredient];
}
void CentralDepot::add_ingredient(core::food::IngredientType ingredient,
                                  int qty_grams, int quality,
                                  Money cost_per_kg) {
  get_stock(ingredient).add_stock(qty_grams, quality, cost_per_kg);
}
bool CentralDepot::consume_ingredient(core::food::IngredientType ingredient,
                                      int qty_grams) {
  return get_stock(ingredient).consume(qty_grams);
}
const core::food::IngredientArray<IngredientStock>&
CentralDepot::all_stocks() const {
  return stocks_;
}
core::food::IngredientArray<IngredientStock>& CentralDepot::all_stocks() {
  return stocks_;
}

const IngredientStock*
CentralDepot::get_ingredient_stock(int ingredient_id) const {
  if (ingredient_id < 0 || ingredient_id >= max_ingredient_count) {
    return nullptr;
  }
  return &stocks_[ingredient_id];
}

IngredientStock* CentralDepot::get_ingredient_stock(int ingredient_id) {
  if (ingredient_id < 0 || ingredient_id >= max_ingredient_count) {
    return nullptr;
  }
  return &stocks_[ingredient_id];
}

const std::vector<core::RestaurantId>&
CentralDepot::delivery_priorities() const {
  return delivery_priorities_;
}

void CentralDepot::sync_delivery_priorities(
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

void CentralDepot::promote_to_top(core::RestaurantId id) {
  auto it =
      std::find(delivery_priorities_.begin(), delivery_priorities_.end(), id);
  if (it != delivery_priorities_.end() && it != delivery_priorities_.begin()) {
    delivery_priorities_.erase(it);
    delivery_priorities_.insert(delivery_priorities_.begin(), id);
  }
}

void CentralDepot::delivery_priorities(
    std::vector<core::RestaurantId> priorities) {
  delivery_priorities_ = std::move(priorities);
}

std::optional<IngredientCategory> CentralDepot::take_stock_stolen() {
  auto stolen = stock_stolen_;
  stock_stolen_.reset();
  return stolen;
}
