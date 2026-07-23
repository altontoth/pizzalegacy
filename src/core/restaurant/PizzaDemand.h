/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <vector>
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

  // The pizza's four defining ingredients: the highest scoring by total amount
  // on the pizza times intensity, taken across all categories, so one category
  // can supply several. Fewer than four entries when the pizza has fewer
  // distinct ingredients.
  static std::vector<food::IngredientType>
  find_primary_ingredients(const core::food::Pizza& pizza);

  // Calculate price/income demand modifier.
  // - Higher prices reduce demand
  // - Higher district income reduces price sensitivity
  static int calculate_price_income_modifier(Money price,
                                             const District& district);
};

} // namespace core::restaurant
