/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/PizzaDemand.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "core/city/City.h"
#include "core/city/District.h"
#include "core/food/IngredientArray.h"
#include "core/food/IngredientType.h"
#include "core/food/Pizza.h"
#include "core/food/PlayerRecipes.h"
#include "core/restaurant/Menu.h"
#include "core/util/ConfigManager.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
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
  const int base_value = menu_entry.quality_base;
  int demo_base = district.demographic_base();
  if (demo_base == 0) {
    demo_base = 1;
  }
  int adjusted = base_value - demo_base;
  adjusted = base_value + (adjusted * district.demographic_factor()) / 200;
  double ratio = static_cast<double>(adjusted) / demo_base;

  // Price/income penalty (can go negative for cheap pizzas = bonus)
  int price_penalty =
      calculate_price_income_modifier(menu_entry.price, district);

  // Final: appeal * district_ratio - price_penalty, truncated once at the end.
  int result = static_cast<int>(
      std::trunc((static_cast<double>(demand) * ratio) - price_penalty));

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
  int total = 0;
  for (auto ingredient_type : find_primary_ingredients(pizza)) {
    total += city.ingredient_trend_system().get_trend(ingredient_type);
  }
  return std::clamp(total, -125, 125);
}

std::vector<food::IngredientType>
PizzaDemand::find_primary_ingredients(const core::food::Pizza& pizza) {
  // Score every ingredient by the total amount of it on the pizza times its
  // intensity, then take the four highest.
  core::food::IngredientArray<int> grams{};
  core::food::IngredientArray<int> intensity{};
  for (const auto& topping : pizza.toppings()) {
    if (!topping || !topping->ingredient()) {
      continue;
    }
    auto ingredient_type = topping->ingredient_type();
    grams[ingredient_type] += topping->weight();
    intensity[ingredient_type] = topping->ingredient()->intensity();
  }

  constexpr int primary_ingredient_count = 4;
  std::vector<food::IngredientType> primary;
  std::vector<bool> taken(max_ingredient_count, false);

  for (int slot = 0; slot < primary_ingredient_count; ++slot) {
    int best_score = 0;
    std::optional<food::IngredientType> best;
    for (int i = 0; i < max_ingredient_count; ++i) {
      if (taken[i] ||
          !core::food::is_valid_ingredient_id(static_cast<uint8_t>(i))) {
        continue;
      }
      auto ingredient_type = static_cast<food::IngredientType>(i);
      int score = grams[ingredient_type] * intensity[ingredient_type];
      if (score > best_score) {
        best_score = score;
        best = ingredient_type;
      }
    }
    if (!best) {
      break;
    }
    taken[static_cast<int>(*best)] = true;
    primary.push_back(*best);
  }

  return primary;
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

  // The curve is truncated to an integer before the cheap-pizza bonus, and the
  // result of that subtraction is truncated again.
  double penalty = std::trunc(std::pow(income_exp, price_norm));


  // When price_norm <= income_curve, cheap pizzas get a demand bonus
  if (price_norm <= income_curve) {
    double adjustment = (income_curve - price_norm) * 5.0;
    penalty = std::trunc(penalty - adjustment);
  }

  // The original converts the overflowing x87 result to INT_MIN, which is then
  // absorbed by the final demand clamp. Avoid undefined double-to-int
  // conversion in C++ while preserving the same observable result for extreme
  // prices. I doubt we can get here with the $50 price limit on pizzas but just
  // to be sure.
  constexpr double penalty_limit = 100000.0;
  return static_cast<int>(std::clamp(penalty, -penalty_limit, penalty_limit));
}

} // namespace core::restaurant
