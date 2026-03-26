/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/PizzaDemand.h"
#include <algorithm>
#include <array>
#include <cmath>
#include "core/city/City.h"
#include "core/city/District.h"
#include "core/food/Pizza.h"
#include "core/food/PlayerRecipes.h"
#include "core/restaurant/Menu.h"
#include "core/util/ConfigManager.h"
#include "engine/Random.h"

namespace core::restaurant {

int PizzaDemand::calculate(const MenuEntry& menu_entry,
                           const core::food::PlayerRecipes& recipes,
                           const City& city, const District& district,
                           ConsumerType demographic) {
  // Look up the pizza from the menu entry's recipe_id
  const auto* recipe = recipes.find(menu_entry.recipe_id);
  if (!recipe) {
    return 0;
  }
  const auto& pizza = recipe->pizza();

  // Base: static pizza popularity for this demographic (0-100)
  int base = pizza.popularity()[demographic];

  // Bonus: ingredient trendiness (-125 to +125)
  int ingredient_bonus = calculate_ingredient_popularity(pizza, city);

  // Combine: scale ingredient bonus by demographic appeal
  // This means popular pizzas (appeal > 50) get amplified trending bonus,
  // unpopular pizzas (appeal < 50) get reduced trending bonus
  int demand = base;
  if (ingredient_bonus > 0) {
    int modifier = ((base * 2) - 100); // -100 to +100 based on base popularity
    demand += ingredient_bonus + ((ingredient_bonus * modifier) / 100);
  }
  else {
    // Negative trends just subtract directly
    demand += ingredient_bonus;
  }

  // District ratio: modulates appeal based on district economics.
  constexpr int BASE_CUSTOMER_VALUE = 20;
  int demo_base = district.demographic_base();
  if (demo_base == 0) {
    demo_base = 1;
  }
  int adjusted = BASE_CUSTOMER_VALUE - demo_base;
  adjusted =
      BASE_CUSTOMER_VALUE + (adjusted * district.demographic_factor()) / 200;
  double ratio = static_cast<double>(adjusted) / demo_base;

  // Price/income penalty (can go negative for cheap pizzas = bonus)
  int price_penalty =
      calculate_price_income_modifier(menu_entry.price, district);

  // Final: appeal * district_ratio - price_penalty
  int result =
      static_cast<int>(static_cast<double>(demand) * ratio) - price_penalty;

  // [Life improvement] Guarantee a small random minimum demand for any pizza
  // on the menu. The original's aggressive demographic scoring (100 - 4*sum)
  // causes most pizzas to get exactly 0 demand for most demographics, making
  // most of the 14 canonical pizzas pointless to have on the menu.
  if (ConfigManager::instance()->options().qol_improvements) {
    result = std::max(result, engine::random::rand_max_n(3));
  }

  return std::clamp(result, 0, 200);
}

int PizzaDemand::calculate_ingredient_popularity(const core::food::Pizza& pizza,
                                                 const City& city) {
  // Sums trend values for primary ingredient of each category (0-3)
  int total = 0;

  // Check each ingredient category
  static constexpr std::array<IngredientCategory, 4> categories = {
      IngredientCategory::VEGETABLES, IngredientCategory::MEAT,
      IngredientCategory::FRUIT, IngredientCategory::CHEESE};

  for (auto category : categories) {
    auto ingredient_type = get_primary_ingredient_for_category(pizza, category);
    if (ingredient_type.has_value()) {
      // Get trend value from city
      total += city.ingredient_trend_system().get_trend(*ingredient_type);
    }
  }

  return std::clamp(total, -125, 125);
}

std::optional<food::IngredientType>
PizzaDemand::get_primary_ingredient_for_category(const core::food::Pizza& pizza,
                                                 IngredientCategory category) {
  // Find the ingredient with the highest weight * intensity for this category.
  // The "primary" ingredient is the one with the most impact (weight *
  // intensity).
  int best_score = 0;
  std::optional<food::IngredientType> best_ingredient;

  for (const auto& topping : pizza.toppings()) {
    if (topping && topping->ingredient() &&
        topping->ingredient()->category() == category) {
      // Calculate impact score: weight x intensity
      int score = topping->weight() * topping->ingredient()->intensity();
      if (score > best_score) {
        best_score = score;
        best_ingredient = topping->ingredient_type();
      }
    }
  }

  return best_ingredient;
}

int PizzaDemand::calculate_price_income_modifier(Money price,
                                                 const District& district) {
  // Normalize price:
  double price_norm = std::max((price.cents - 400) / 100.0, 1.0);

  // Get district income factor (raw value, not the calculated average_income)
  auto income = static_cast<double>(district.average_income_factor());

  // Income curve parameters
  double income_curve = 8.0 - (income / 25.0); // Higher income = lower curve
  double income_exp = (income / 250.0) + 1.3;  // Higher income = higher exp

  double pow_result = std::pow(income_exp, price_norm);

  // When price_norm <= income_curve, cheap pizzas get a demand bonus
  double penalty = pow_result;
  if (price_norm <= income_curve) {
    double adjustment = (income_curve - price_norm) * 5.0;
    penalty = pow_result - adjustment;
  }

  return static_cast<int>(penalty);
}

} // namespace core::restaurant
