/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/ManagerRoutine.h"
#include <algorithm>
#include <array>
#include <string_view>
#include "core/city/City.h"
#include "core/city/Property.h"
#include "core/food/IngredientCategoryType.h"
#include "core/food/IngredientType.h"
#include "core/logistics/IngredientStock.h"
#include "core/restaurant/ManagerFeedback.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/RestaurantStats.h"
#include "core/state/GameState.h"
#include "core/status/StatusHub.h"
#include "core/supply/FoodSupplier.h"
#include "core/time/TimeOfDay.h"
#include "engine/Logger.h"
#include "engine/Random.h"

using namespace core::restaurant;
using engine::random::rand_max_n;
using engine::random::rand_percentage_chance;

// Manager price adjustment steps (cents)
static constexpr Money price_adjust_up = 30_c;
static constexpr Money price_adjust_down = 40_c;

// Ingredient type index ranges per category (start index, count)
struct CategoryRange {
  int start;
  int count;
};
static constexpr std::array<CategoryRange, 4> CATEGORY_RANGES{{
    {.start = 0, .count = 22},
    {.start = 24, .count = 17},
    {.start = 48, .count = 13},
    {.start = 72, .count = 13},
}};

namespace {
const CategoryRange& category_range(IngredientCategory category) {
  return CATEGORY_RANGES[static_cast<int>(category)];
}

constexpr int opening_block = Restaurant::DEFAULT_OPENING_TIME.to_block();
} // namespace

ManagerRoutine::ManagerRoutine(Restaurant& restaurant, GameState& game_state)
    : restaurant_(restaurant), game_state_(game_state) {}

void ManagerRoutine::queue_alert(std::string message) {
  queue_alert(std::move(message), opening_block);
}

void ManagerRoutine::queue_alert(std::string message, int display_block) {
  game_state_.turn().queue_manager_alert(restaurant_.id(), std::move(message),
                                         display_block);
}

void ManagerRoutine::run_daily() {
  auto& manager = restaurant_.manager();
  if (!manager) {
    return;
  }

  // Availability check
  if (manager->is_away()) {
    Logger::info("[Manager] {} - Manager unavailable (away)",
                 restaurant_.name());
    return;
  }

  report_previous_day_shortages();

  // Activity gate: motivation% chance to be active today
  if (!rand_percentage_chance(manager->character().motivation())) {
    Logger::info("[Manager] {} - Manager idle today (motivation check failed)",
                 restaurant_.name());
    return;
  }

  do_shopping();

  // High management = more likely to continue.
  int laziness = 100 - manager->character().management();
  if (rand_percentage_chance(laziness)) {
    Logger::info("[Manager] {} - Manager skipped remaining duties",
                 restaurant_.name());
    return;
  }

  do_price_adjustment();
  do_turnover_monitoring();

  // Staff behavior influence
  ManagerFeedback feedback(&restaurant_);
  feedback.daily_adjustments();
}

void ManagerRoutine::report_previous_day_shortages() {
  using core::character::JobRole;

  auto filled_in_as = restaurant_.manager_filled_in_as();
  if (filled_in_as != JobRole::NONE) {
    const std::string_view role =
        (filled_in_as == JobRole::CHEF) ? "cooks" : "waiters";
    queue_alert(compat::format("Boss! Yesterday in restaurant {} I had to help "
                               "out our {} with their work!",
                               restaurant_.name(), role));
    restaurant_.set_manager_filled_in_as(JobRole::NONE);
  }

  auto shortage = restaurant_.manager_staff_shortage();
  if (shortage != JobRole::NONE) {
    const std::string_view role =
        (shortage == JobRole::CHEF) ? "cooks" : "waiters";
    queue_alert(
        compat::format("Boss! In restaurant {} we haven't got enough {}!",
                       restaurant_.name(), role));
    restaurant_.set_manager_staff_shortage(JobRole::NONE);
  }
}

// Counts ingredients in the given category that have less than three days'
// worth of stock based on cached daily demand. Ingredients with no daily
// demand are ignored.
int ManagerRoutine::count_category_shortages(const Restaurant& restaurant,
                                             IngredientCategory category) {
  const auto& range = category_range(category);
  const auto& daily_demand = restaurant.daily_ingredient_demand_cache();

  int shortage_count = 0;
  for (int i = 0; i < range.count; ++i) {
    auto ingredient_type =
        static_cast<core::food::IngredientType>(range.start + i);
    int daily_grams = daily_demand[ingredient_type];
    if (daily_grams <= 0) {
      continue;
    }
    int current = restaurant.inventory().get_stock(ingredient_type).quantity;
    if (current < daily_grams * 3) {
      ++shortage_count;
    }
  }
  return shortage_count;
}

void ManagerRoutine::queue_shortage_warning(IngredientCategory category,
                                            int shortage_count) {
  std::string_view order_word = (shortage_count == 1) ? "order" : "orders";
  queue_alert(compat::format("Boss! We urgently need {} {} of {} for {}",
                             shortage_count, order_word, to_string(category),
                             restaurant_.name()),
              opening_block + static_cast<int>(category));
}

void ManagerRoutine::do_shopping() {
  if (!restaurant_.wants_open()) {
    return;
  }
  const auto& settings = restaurant_.manager_settings();
  const auto& manager = restaurant_.manager();
  if (!manager) {
    return;
  }

  if (settings.supply_shortage == ManagerReaction::DoNothing ||
      (!settings.shopping_if_needed &&
       settings.supply_shortage != ManagerReaction::CallMe)) {
    return;
  }

  const auto& city = restaurant_.property().city();

  auto* player = game_state_.active_player();
  if (!player) {
    return;
  }

  const auto& daily_demand = restaurant_.daily_ingredient_demand_cache();

  static constexpr std::array<float, 5> difficulty_multipliers = {
      0.6f, 0.8f, 1.0f, 1.5f, 1.9f};
  float difficulty_mult =
      difficulty_multipliers[game_state_.difficulty_level() - 1];

  for (int cat_index = 0; cat_index < Restaurant::NUM_INGREDIENT_CATEGORIES;
       ++cat_index) {
    auto category = static_cast<IngredientCategory>(cat_index);
    const auto& range = category_range(category);

    int shortage_count = count_category_shortages(restaurant_, category);
    if (shortage_count == 0) {
      continue;
    }

    if (settings.supply_shortage == ManagerReaction::CallMe) {
      queue_shortage_warning(category, shortage_count);
      continue;
    }

    if (!settings.shopping_if_needed) {
      continue;
    }

    // Find the preferred supplier, falling back to first available
    const FoodSupplier* supplier = nullptr;
    auto preferred_id = restaurant_.preferred_suppliers()[cat_index];
    for (const auto& s : city.suppliers()) {
      if (s->category() != category) {
        continue;
      }
      if (!preferred_id || s->id() == preferred_id) {
        supplier = s.get();
        if (s->id() == preferred_id) {
          break; // Found exact match
        }
      }
    }
    if (!supplier) {
      continue;
    }

    // Buy 2 days of demand for each low ingredient
    Money category_total{0};
    int category_qty = 0;
    int best_discount = 0;

    for (int i = 0; i < range.count; ++i) {
      auto ingredient_type =
          static_cast<core::food::IngredientType>(range.start + i);
      int daily_grams = daily_demand[ingredient_type];
      if (daily_grams <= 0) {
        continue;
      }
      int current = restaurant_.inventory().get_stock(ingredient_type).quantity;
      if (current >= daily_grams * 3) {
        continue;
      }

      int buy_kg = (daily_grams * 2 + 999) / 1000;
      if (buy_kg == 0) {
        continue;
      }

      Money base_price = city.ingredient_base_price(ingredient_type);
      Money price_per_kg = supplier->calculate_price(
          ingredient_type, base_price, difficulty_mult);

      // Negotiation: experience% chance to get a discount up to motivation/2 %
      if (rand_percentage_chance(manager->character().experience())) {
        int discount = rand_max_n(manager->character().motivation() / 2);
        price_per_kg = Money{price_per_kg.cents * (100 - discount) / 100};
        best_discount = std::max(discount, best_discount);
      }

      Money total_cost{price_per_kg.cents * buy_kg};

      if (!settings.overdraw_account &&
          player->financials().balance(Account::Cash) < total_cost) {
        continue;
      }

      player->financials().spend_cash(game_state_.time().current_date(),
                                      Account::Stock, total_cost,
                                      restaurant_.id());
      restaurant_.inventory().add_ingredient(ingredient_type, buy_kg * 1000,
                                             supplier->quality(), price_per_kg);
      category_total = category_total + total_cost;
      category_qty += buy_kg;
    }

    if (category_qty > 0) {
      game_state_.status().post(StatusMsg{
          .payload = compat::format(
              "Your manager goes shopping for {} and buys {} at {} for {} "
              "with {}% discount",
              restaurant_.name(), to_string(category), supplier->name(),
              category_total, best_discount),
          .severity = StatusSeverity::Info,
          .scope = StatusScope::Restaurant,
      });
    }
  }
}

void ManagerRoutine::do_price_adjustment() {
  const auto& settings = restaurant_.manager_settings();
  if (settings.turnover_decrease != ManagerReaction::ActYourself &&
      settings.turnover_increase != ManagerReaction::ActYourself) {
    return;
  }

  const auto& manager = restaurant_.manager();
  if (!manager) {
    return;
  }
  int management = manager->character().management();

  // Flat increase pass: if today's fulfillment rate < 80%, raise all prices.
  // The restaurant can't keep up with demand, so reduce demand pressure by
  // raising prices.
  if (restaurant_.fulfillment_rate() < 80 && settings.change_prices) {
    for (auto& entry : restaurant_.menu().entries()) {
      entry.price = std::min(entry.price + 100_c, Menu::max_price);
    }
  }

  // Smart per-pizza adjustment pass, over the last settled day.
  const auto& traded = restaurant_.stats().yesterday;

  for (auto& entry : restaurant_.menu().entries()) {
    auto it = traded.pizza_sales.find(entry.recipe_id);
    int sold = (it != traded.pizza_sales.end()) ? it->second.count_sold : 0;
    int ordered =
        (it != traded.pizza_sales.end()) ? it->second.count_ordered : 0;

    // Gate: if management > 70, skip when there is unmet demand.
    if (ordered != sold && management > 70) {
      continue;
    }

    // Choose demand figure based on management level
    // Low management: uses "actually sold" figure
    // High management: uses total demand (ordered)
    int demand_figure = ordered;
    if (management < 30) {
      demand_figure = sold;
    }

    // pizza[+1] * pizza[+2] = cost estimate.
    // pizza[+1] is unknown, appears always 0 in the original game.
    int cost_estimate = 0;

    int price_cents = static_cast<int>(entry.price.cents);
    int revenue_estimate = demand_figure * price_cents;

    if (management > 60 && it != traded.pizza_sales.end() && sold > 0) {
      // High-management: factor in per-portion costs for a more accurate
      // profit signal. Dampens the revenue estimate.
      int cost_per_portion =
          static_cast<int>(it->second.ingredient_cost.cents) / sold;
      cost_estimate -= sold * cost_per_portion;
      revenue_estimate -= cost_per_portion * demand_figure;
    }

    int profit = revenue_estimate - cost_estimate;

    if (settings.change_prices) {
      if (profit < -5000) {
        entry.price =
            std::max(entry.price - price_adjust_down, Menu::min_price);
      }
      else if (profit > 2000) {
        entry.price = std::min(entry.price + price_adjust_up, Menu::max_price);
      }
    }
  }
}

void ManagerRoutine::do_turnover_monitoring() {
  const auto& settings = restaurant_.manager_settings();
  if (settings.turnover_decrease == ManagerReaction::DoNothing &&
      settings.turnover_increase == ManagerReaction::DoNothing) {
    return;
  }

  const auto& manager = restaurant_.manager();
  if (!manager) {
    return;
  }
  int management = manager->character().management();

  // The last settled day against the one before it.
  const auto& traded = restaurant_.stats().yesterday;
  const auto& previous = restaurant_.stats().day_before_yesterday;

  int revenue_diff =
      static_cast<int>(traded.revenue.cents - previous.revenue.cents);

  if (management > 60) {
    // Profit-aware mode: subtract the cost change from the revenue change
    int cost_diff = static_cast<int>(traded.ingredient_costs.cents -
                                     previous.ingredient_costs.cents);
    revenue_diff -= cost_diff;
  }

  // Thresholds in cents.
  // TODO REVIEW: the original tests the decrease setting against a $500 rise
  // and the increase setting against anything below a $200 rise, so one of the
  // two setting labels or the two strings is swapped there. Ours requires a
  // $200 fall for the second test, making it the rarer of the two.
  constexpr int INCREASE_THRESHOLD = 50000;
  constexpr int DECREASE_THRESHOLD = 20000;

  const char* trend_word = (management > 60) ? "profit" : "turnover";

  if (settings.turnover_decrease == ManagerReaction::CallMe &&
      revenue_diff > INCREASE_THRESHOLD) {
    queue_alert(compat::format("Boss! In restaurant no. {} the {} is going up.",
                               restaurant_.id().value, trend_word));
  }

  if (settings.turnover_increase == ManagerReaction::CallMe &&
      revenue_diff < -DECREASE_THRESHOLD) {
    queue_alert(
        compat::format("Boss! In restaurant no. {} the {} is going down.",
                       restaurant_.id().value, trend_word));
  }
}
