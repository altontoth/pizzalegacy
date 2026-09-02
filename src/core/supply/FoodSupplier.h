/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <array>
#include <optional>
#include <unordered_map>
#include "core/character/Actor.h"
#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/food/IngredientCategoryType.h"
#include "core/food/IngredientType.h"
#include "core/supply/SupplierRelationship.h"

constexpr int VEGETABLE_COUNT = 22;
constexpr int MEAT_COUNT = 17;
constexpr int FRUIT_COUNT = 13;
constexpr int CHEESE_COUNT = 13;

class FoodSupplier {
public:
  FoodSupplier(IngredientCategory category, uint8_t supplier_id,
               uint8_t quality, std::string name, uint8_t head_id,
               Money min_contract, const uint8_t* price_level,
               int price_level_count);
  Money minimum_contract_amount() const;
  int number_of_products() const;
  ActorType actor_type() const;
  uint8_t id() const;
  uint8_t quality() const;
  const std::string& name() const;
  uint8_t head_id() const;
  uint8_t chance_of_no_delivery() const;

  SupplierRelationship& relationship(core::PlayerId player_id) const;
  int spending_rank(core::PlayerId player_id) const;

  // Get price level for a specific ingredient ID
  uint8_t
  price_level_for_ingredient(core::food::IngredientType ingredient_type) const;

  bool sells(core::food::IngredientType ingredient_type) const;

  // Final price for an ingredient, empty when the supplier does not sell it.
  std::optional<Money>
  calculate_price(core::food::IngredientType ingredient_type, Money base_price,
                  float difficulty_multiplier) const;

  // The city's base price with the difficulty adjustment, before any
  // supplier's own price level is applied.
  static Money city_price(Money base_price, float difficulty_multiplier);

  // Impact of difficulty level on prices
  static constexpr float price_multiplier(int difficulty_level) {
    constexpr std::array<float, 5> multipliers = {0.6f, 0.8f, 1.0f, 1.5f, 1.9f};
    return multipliers[static_cast<std::size_t>(
        std::clamp(difficulty_level - 1, 0, 4))];
  }

  // Get/set stock for an ingredient
  int stock(core::food::IngredientType ingredient_type) const;
  void set_stock(core::food::IngredientType ingredient_type, int quantity);

  // Get the ingredient category this supplier handles (alias for type())
  IngredientCategory category() const { return category_; }

private:
  int category_index(core::food::IngredientType ingredient_type) const;
  IngredientCategory category_;
  uint8_t supplier_id_;
  uint8_t quality_;
  std::string name_;
  ActorType actor_type_;
  uint8_t head_id_;
  Money min_contract_amount_;
  std::array<uint8_t, 22> price_levels_; // Max 22 for vegetables
  std::array<int, 22> stock_levels_{};   // Stock levels in kg
  mutable std::unordered_map<core::PlayerId, SupplierRelationship>
      relationships_;
};
