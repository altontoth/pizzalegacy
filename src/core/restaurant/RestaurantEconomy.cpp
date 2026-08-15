/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/RestaurantEconomy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include "core/Demographics.h"
#include "core/city/City.h"
#include "core/city/Property.h"
#include "core/food/PlayerRecipes.h"
#include "core/restaurant/Menu.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "engine/Random.h"

namespace {
int calculate_competition_penalty(const Restaurant& competitor,
                                  int competitor_base_demand, int distance) {
  int competitor_range = competitor.economy().range_tiles();

  // If competitor is out of range, no penalty
  if ((competitor_range + 1) < distance) {
    return 0;
  }

  int penalty = competitor_base_demand - (distance * 4);

  // If penalty would be negative (competitor too far to matter), return 0
  if (penalty < 0) {
    return 0;
  }
  return penalty;
}
} // anonymous namespace

// This updates the restaurant range every day, but range_tiles =
// sqrt(range_raw), so the tile range grows much slower than the raw value
bool RestaurantEconomy::update_restaurant_range() {
  int previous_range_tiles = range_tiles_;

  // Calculate range_tiles from range_raw (sqrt)
  range_tiles_ = static_cast<int>(std::sqrt(range_raw_));

  if (range_raw_ < 120) {
    if (engine::random::rand_percentage_chance(50)) {
      range_raw_++;
    }
  }

  // Cap range_raw at 200
  range_raw_ = std::min<int>(range_raw_, 200);

  // Recalculate range_tiles after potential increment
  range_tiles_ = static_cast<int>(std::sqrt(range_raw_));

  return range_tiles_ > previous_range_tiles;
}

void RestaurantEconomy::calculate_competitive_strength(
    const Restaurant& restaurant, const core::food::PlayerRecipes* recipes) {
  for (ConsumerType demographic : ALL_CONSUMER_TYPES) {
    int base = furniture_appeal_[demographic] + total_popularity_;

    int value = base * menu_factor_[demographic];

    value = std::max(value, 0);

    int sqrt_value = static_cast<int>(std::sqrt(value));

    // Apply quality rating
    int threshold = (sqrt_value * quality_rating_) / 100;

    // Copy protection penalty: Check if using at least 3 canonical recipes
    if (recipes) {
      int canonical_count = 0;
      int imitation_sum = 0;

      for (const auto& entry : restaurant.menu().entries()) {
        const auto* recipe = recipes->find(entry.recipe_id);
        if (recipe && recipe->canonical_id().has_value()) {
          canonical_count++;
          imitation_sum += recipe->pizza().imitation();
        }
      }

      if (canonical_count > 0) {
        avg_canonical_imitation_ = imitation_sum / canonical_count;
      }

      bool apply_penalty = canonical_count < 3 || avg_canonical_imitation_ < 30;

      if (apply_penalty) {
        threshold /= 8;
      }
    }

    threshold = std::clamp(threshold, 1, 250);

    competitive_strength_[demographic] = threshold;
  }
}

void RestaurantEconomy::calculate_furniture_appeal(
    const Restaurant& restaurant) {
  auto bonused = calculate_furniture_appeal_popularity(restaurant);
  for (ConsumerType demographic : ALL_CONSUMER_TYPES) {
    furniture_appeal_[demographic] = static_cast<int>(bonused[demographic]);
  }
}

Popularity RestaurantEconomy::calculate_menu_popularity(
    const Restaurant& restaurant,
    const core::food::PlayerRecipes& recipes) const {
  Popularity result;

  DemographicArray<float> menu_appeal{};
  int menu_size = 0;

  for (const auto& entry : restaurant.menu().entries()) {
    const auto* recipe = recipes.find(entry.recipe_id);
    if (!recipe) {
      continue;
    }

    const auto& pizza = recipe->pizza();
    const auto& popularity = pizza.popularity();

    menu_size++;
    for (ConsumerType demographic : ALL_CONSUMER_TYPES) {
      menu_appeal[demographic] += static_cast<float>(popularity[demographic]);
    }
  }

  for (ConsumerType demographic : ALL_CONSUMER_TYPES) {
    float avg_appeal =
        menu_size > 0 ? menu_appeal[demographic] / static_cast<float>(menu_size)
                      : 50.0f;
    result[demographic] = static_cast<int>(avg_appeal);
  }

  return result;
}

Popularity RestaurantEconomy::calculate_furniture_popularity(
    const Restaurant& restaurant) const {
  Popularity result;

  const auto& layout = restaurant.layout();
  const auto& floor_pattern = layout->floor_pattern();

  for (auto demographic : ALL_CONSUMER_TYPES) {
    int floor_pop = floor_pattern.popularity_for(demographic);

    int furniture_pop = 0;
    if (layout) {
      for (const auto& placed : layout->placed_furniture()) {
        if (!placed || !placed->furniture()) {
          continue;
        }

        furniture_pop += placed->furniture()->popularity(demographic);
      }
    }

    result[demographic] = floor_pop + furniture_pop;
  }

  return result;
}

Popularity RestaurantEconomy::calculate_furniture_appeal_popularity(
    const Restaurant& restaurant) const {
  auto result = calculate_furniture_popularity(restaurant);

  for (ConsumerType demographic : ALL_CONSUMER_TYPES) {
    const int demo_index = static_cast<int>(demographic);
    int value = result[demographic];

    if (jukebox_music_ > 0) {
      value += (jukebox_music_ - 1 == demo_index) ? 12 : -4;
    }

    if (tv_channel_ > 0) {
      value += (tv_channel_ - 1 == demo_index) ? 12 : -4;
    }

    // Floor, furniture and media contributions are halved together.
    result[demographic] = std::clamp(value / 2, 0, 150);
  }

  return result;
}

// Scans all restaurants from all players, applies distance-based competition
void RestaurantEconomy::calculate_demand_with_competition(
    const Restaurant& restaurant, GameState& game_state) {

  const auto& property = restaurant.property();

  const City& city = property.city();
  int property_index = property.index();
  int center_x = City::index_to_x(property_index);
  int center_y = City::index_to_y(property_index);
  int search_radius = range_tiles_ + 10;

  int min_x = std::max(0, center_x - search_radius);
  int max_x = std::min(159, center_x + search_radius); // Map width 160
  int min_y = std::max(0, center_y - search_radius);
  int max_y = std::min(119, center_y + search_radius); // Map height 120

  std::array<int, 6> competition_scores = {100, 100, 100, 100, 100, 100};

  int competitor_count = 0;
  int strongest_competitor_penalty = 0;
  std::optional<core::RestaurantId> strongest_competitor;

  // Competition is per-city. Every city reuses the same tile grid, so scanning
  // other cities would count a restaurant whose coordinates land in the search
  // box as a phantom competitor.
  for (auto& player : game_state.players()) {
    auto player_restaurants =
        game_state.restaurants_in_city(player->id(), city.id());

    for (auto* comp_restaurant : player_restaurants) {
      if (comp_restaurant->id() == restaurant.id()) {
        continue;
      }

      if (!comp_restaurant->wants_open() ||
          comp_restaurant->is_force_closed()) {
        continue;
      }

      const auto& comp_property = comp_restaurant->property();

      int comp_index = comp_property.index();
      int comp_x = City::index_to_x(comp_index);
      int comp_y = City::index_to_y(comp_index);

      if (comp_x < min_x || comp_x > max_x || comp_y < min_y ||
          comp_y > max_y) {
        continue;
      }

      int distance = std::abs(center_x - comp_x) + std::abs(center_y - comp_y);

      int total_penalty = 0;
      for (ConsumerType demo : ALL_CONSUMER_TYPES) {
        int comp_base_demand =
            comp_restaurant->economy().competitive_strength()[demo];
        int penalty = calculate_competition_penalty(*comp_restaurant,
                                                    comp_base_demand, distance);
        competition_scores[static_cast<int>(demo)] += penalty;
        total_penalty += penalty;
      }

      if (total_penalty > strongest_competitor_penalty) {
        strongest_competitor_penalty = total_penalty;
        strongest_competitor = comp_restaurant->id();
      }

      competitor_count++;
    }
  }

  // Store competitor data
  competitor_count_ = competitor_count;
  strongest_competitor_ = strongest_competitor;

  int total_competition = 0;
  for (int score : competition_scores) {
    total_competition += score;
  }
  competition_score_ = total_competition - 600;

  // Apply competition to calculate actual customer counts
  const auto& property_demand = property.potential_customers();

  for (ConsumerType demo : ALL_CONSUMER_TYPES) {
    int our_base_demand = competitive_strength_[demo];
    int comp_score = competition_scores[static_cast<size_t>(demo)];
    int prop_demand = property_demand[demo];

    int customer_count;
    if (our_base_demand >= comp_score) {
      // We dominate - get full property demand
      customer_count = prop_demand;
    }
    else {
      // Competition affects us - scale down proportionally
      customer_count = (prop_demand * our_base_demand) / comp_score;
    }

    customer_count = (customer_count * avg_canonical_imitation_) / 100;

    customer_counts_[demo] = customer_count;
  }

  // Apply capacity limiting
  int total_customers = 0;
  for (int count : customer_counts_) {
    total_customers += count;
  }

  const auto owner = property.owner_or_tenant();
  const Player* owner_player = owner ? game_state.player(*owner) : nullptr;
  int daily_capacity = 0;
  if (owner_player && !owner_player->is_human_controlled()) {
    daily_capacity = restaurant.ai_customers_per_day();
  }
  else {
    auto service_time = restaurant.calculate_service_time_data();
    daily_capacity = restaurant.calculate_seating_capacity(&service_time);
  }

  if (total_customers > daily_capacity && daily_capacity > 0) {
    int scale = (daily_capacity * 100) / total_customers;
    for (ConsumerType demo : ALL_CONSUMER_TYPES) {
      customer_counts_[demo] = (customer_counts_[demo] * scale) / 100;
    }
    rejection_rate_ = 100 - scale;
  }
  else {
    rejection_rate_ = 0;
  }

  // Local share: Calculate our share of demand relative to nearby competition.
  // With no competitors, competition_scores stays at 100 per demographic,
  // giving a baseline denominator of 600.
  int total_base_demand = 0;
  for (int strength : competitive_strength_) {
    total_base_demand += strength;
  }
  int demand_ratio_calc = 0;
  if (total_competition > 0) {
    demand_ratio_calc = (total_base_demand * 100) / total_competition;
  }
  demand_ratio_ = std::clamp(demand_ratio_calc, 0, 100);
}
