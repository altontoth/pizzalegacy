/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include "core/Demographics.h"
#include "core/common/Ids.h"

namespace core::food {
struct PlayerRecipes;
}

class Restaurant;
class GameState;

/**
 * Manages restaurant economic state
 *
 * Owns range, furniture appeal, menu factor, competitive strength,
 * customer counts, and competition data. Restaurant owns popularity
 * (the 8 components + total) and fulfillment rate, and pushes
 * total_popularity into this class for use in demand calculations.
 *
 * Daily pipeline, in Restaurant::end_of_day while the restaurant is open:
 *
 *     update_restaurant_range()
 *     calculate_demand_with_competition() => customer_counts
 *     Restaurant calculates popularity components => total_popularity
 *     calculate_furniture_appeal()
 *     calculate_competitive_strength()
 *     Restaurant distributes customers to pizzas (also sets menu_factor)
 *     Restaurant fulfills orders => fulfillment_rate
 *     Restaurant processes reputation decay
 */
class RestaurantEconomy {
public:
  RestaurantEconomy() = default;

  // Restaurant's reach in tiles, grows over time
  // range_raw grows 0-200, range_tiles = sqrt(range_raw)
  int range_raw() const { return range_raw_; }
  void range_raw(int raw) { range_raw_ = raw; }
  int range_tiles() const { return range_tiles_; }
  void range_tiles(int tiles) { range_tiles_ = tiles; }

  int total_popularity() const { return total_popularity_; }
  void total_popularity(int pop) { total_popularity_ = pop; }

  FurnitureAppeal& furniture_appeal() { return furniture_appeal_; }
  const FurnitureAppeal& furniture_appeal() const { return furniture_appeal_; }

  MenuFactor& menu_factor() { return menu_factor_; }
  const MenuFactor& menu_factor() const { return menu_factor_; }

  // Actual customers per demographic after competition and capacity limiting
  CustomerCounts& customer_counts() { return customer_counts_; }
  const CustomerCounts& customer_counts() const { return customer_counts_; }
  int total_customers() const {
    int total = 0;
    for (int count : customer_counts_) {
      total += count;
    }
    return total;
  }

  int demand_ratio() const { return demand_ratio_; }
  void demand_ratio(int ratio) { demand_ratio_ = ratio; }

  // Competitor tracking
  int competitor_count() const { return competitor_count_; }
  void competitor_count(int count) { competitor_count_ = count; }

  // The competitor taking the most business off this restaurant, which is the
  // one an AI owner attacks.
  std::optional<core::RestaurantId> strongest_competitor() const {
    return strongest_competitor_;
  }
  void strongest_competitor(std::optional<core::RestaurantId> id) {
    strongest_competitor_ = id;
  }

  // Total competition score minus 600 baseline
  int competition_score() const { return competition_score_; }
  void competition_score(int16_t score) { competition_score_ = score; }

  CompetitiveStrength& competitive_strength() { return competitive_strength_; }
  const CompetitiveStrength& competitive_strength() const {
    return competitive_strength_;
  }

  int quality_rating() const { return quality_rating_; }
  // Daily growth stops at 100; the weekly settlement can carry it to 200.
  void quality_rating(int rating) {
    quality_rating_ = std::clamp(rating, 0, 200);
  }

  int rejection_rate() const { return rejection_rate_; }
  void rejection_rate(int rate) { rejection_rate_ = rate; }

  int jukebox_music() const { return jukebox_music_; }
  void jukebox_music(int music) { jukebox_music_ = music; }
  int tv_channel() const { return tv_channel_; }
  void tv_channel(int channel) { tv_channel_ = channel; }

  bool update_restaurant_range();

  // Calculate demographic thresholds from furniture_appeal, total_popularity,
  // and menu_factor
  void calculate_competitive_strength(
      const Restaurant& restaurant,
      const core::food::PlayerRecipes* recipes = nullptr);

  void calculate_furniture_appeal(const Restaurant& restaurant);

  void calculate_demand_with_competition(const Restaurant& restaurant,
                                         class GameState& game_state);

  Popularity
  calculate_menu_popularity(const Restaurant& restaurant,
                            const core::food::PlayerRecipes& recipes) const;

  Popularity calculate_furniture_popularity(const Restaurant& restaurant) const;

  Popularity
  calculate_furniture_appeal_popularity(const Restaurant& restaurant) const;

private:
  int range_raw_{50};
  int range_tiles_{7};

  int total_popularity_{0};

  // Furniture appeal per demographic
  FurnitureAppeal furniture_appeal_{{1, 1, 1, 1, 1, 1}};

  // Menu demographic factor
  MenuFactor menu_factor_{{1, 1, 1, 1, 1, 1}};

  // Actual customer counts per demographic
  CustomerCounts customer_counts_{{0, 0, 0, 0, 0, 0}};

  // Competitive strength per demographic
  CompetitiveStrength competitive_strength_{{1, 1, 1, 1, 1, 1}};

  int quality_rating_{70};

  // percentage of customers turned away
  int rejection_rate_{0};

  // Competition data
  int demand_ratio_{0};
  std::optional<core::RestaurantId> strongest_competitor_;
  int competitor_count_{0};
  int competition_score_{0};

  int jukebox_music_{0};
  int tv_channel_{0};
};
