/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include "core/Demographics.h"
#include "core/finance/Money.h"
#include "core/food/Ingredient.h"

class City;
class District;

namespace core::food {
class Pizza;
struct PlayerRecipes;
} // namespace core::food

namespace core::restaurant {

struct MenuEntry;

class PizzaDemand {
public:
  // Calculate demand for a pizza for a specific demographic.
  // Combines
  // - Base popularity: Static per-demographic score from recipe (0-100)
  // - Ingredient bonus: Trendiness of ingredients in this city (-125 to +125)
  // - District ratio: Modulates appeal by district economic factors
  // - Price penalty: Exponential price sensitivity based on district income
  static int calculate(const MenuEntry& menu_entry,
                       const core::food::PlayerRecipes& recipes,
                       const City& city, const District& district,
                       ConsumerType demographic);

  // Calculate ingredient trendiness bonus for a pizza.
  // For every type of ingredient we sum the trend values
  static int calculate_ingredient_popularity(const core::food::Pizza& pizza,
                                             const City& city);

  // Get the primary ingredient ID (the one with the highest weight * intensity
  // score for that category) of a given category from a pizza.
  static std::optional<food::IngredientType>
  get_primary_ingredient_for_category(const core::food::Pizza& pizza,
                                      IngredientCategory category);

  // Calculate price/income demand modifier.
  // - Higher prices reduce demand
  // - Higher district income reduces price sensitivity
  static int calculate_price_income_modifier(Money price,
                                             const District& district);
};

} // namespace core::restaurant
