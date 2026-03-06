/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstdint>
#include "core/Demographics.h"

namespace core::food {
struct PlayerRecipes;
}

// Forward declaration
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
 * Daily pipeline (called from Restaurant::start_of_day / end_of_day):
 *
 *   start_of_day:
 *     Restaurant calculates popularity components => total_popularity
 *     calculate_furniture_appeal()
 *     calculate_competitive_strength()
 *
 *   end_of_day:
 *     update_restaurant_range()
 *     Restaurant processes reputation decay
 *     sync total_popularity (captures mid-day reputation changes)
 *     calculate_demand_with_competition() => customer_counts
 *     Restaurant distributes customers to pizzas (also sets menu_factor)
 *     Restaurant fulfills orders => fulfillment_rate
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

  int16_t total_popularity() const { return total_popularity_; }
  void total_popularity(int16_t pop) { total_popularity_ = pop; }

  FurnitureAppeal& furniture_appeal() { return furniture_appeal_; }
  const FurnitureAppeal& furniture_appeal() const { return furniture_appeal_; }

  // Set by Restaurant::calculate_daily_pizza_sales_distribution()
  MenuFactor& menu_factor() { return menu_factor_; }
  const MenuFactor& menu_factor() const { return menu_factor_; }

  // Actual customers per demographic after competition and capacity limiting
  CustomerCounts& customer_counts() { return customer_counts_; }
  const CustomerCounts& customer_counts() const { return customer_counts_; }

  int demand_ratio() const { return demand_ratio_; }
  void demand_ratio(int ratio) { demand_ratio_ = ratio; }

  // Competitor tracking
  int competitor_count() const { return competitor_count_; }
  void competitor_count(int count) { competitor_count_ = count; }

  // Strongest competitor location
  int strongest_competitor_x() const { return strongest_competitor_x_; }
  void strongest_competitor_x(int x) { strongest_competitor_x_ = x; }
  int strongest_competitor_y() const { return strongest_competitor_y_; }
  void strongest_competitor_y(int y) { strongest_competitor_y_ = y; }

  // Total competition score minus 600 baseline
  int competition_score() const { return competition_score_; }
  void competition_score(int16_t score) { competition_score_ = score; }

  // Competitive strength per demographic (+0x21-0x26, called "base_demand" in
  // asm) Higher values = win more customers when competing with nearby
  // restaurants Range: [1, 250] Formula: sqrt((furniture_appeal +
  // total_popularity) * menu_factor) * quality / 100
  CompetitiveStrength& competitive_strength() { return competitive_strength_; }
  const CompetitiveStrength& competitive_strength() const {
    return competitive_strength_;
  }

  // === Quality Rating (+0x29) ===
  // Overall restaurant quality, used in threshold formula
  // Init: 70, Range: [0, 100]
  int quality_rating() const { return quality_rating_; }
  void quality_rating(int rating) { quality_rating_ = rating; }

  // === Rejection Rate ===
  // Percentage of customers turned away due to capacity (0-100)
  int rejection_rate() const { return rejection_rate_; }
  void rejection_rate(int rate) { rejection_rate_ = rate; }

  // === Jukebox/TV Settings (+0x3BB, +0x3BC) ===
  // 0 = none, 1-6 = targeting specific demographic
  int jukebox_music() const { return jukebox_music_; }
  void jukebox_music(int music) { jukebox_music_ = music; }
  int tv_channel() const { return tv_channel_; }
  void tv_channel(int channel) { tv_channel_ = channel; }

  // === Calculation Methods ===

  // Update restaurant range (grows over time)
  // Called daily from Restaurant::end_of_day
  // Returns true if range increased
  bool update_restaurant_range();

  // Calculate demographic thresholds from furniture_appeal, total_popularity,
  // and menu_factor. Formula: sqrt((furniture_appeal + total_popularity) *
  // menu_factor) * quality_rating / 100
  void calculate_competitive_strength(
      const Restaurant& restaurant,
      const core::food::PlayerRecipes* recipes = nullptr);

  // Calculate furniture appeal per demographic (+0x51h)
  // Uses furniture stats plus jukebox/TV bonuses
  // Implements per_demographic_calculator.asm
  void calculate_furniture_appeal(const Restaurant& restaurant);

  // Calculate demand with competition from nearby restaurants
  // From calculate_demand_with_competition.asm
  // Scans all restaurants from all players, applies distance-based penalties
  // Updates customer_counts, competitor_count, competition_score, demand_ratio
  void calculate_demand_with_competition(const Restaurant& restaurant,
                                         class GameState& game_state);

  // === UI Utility Methods (const, don't modify state) ===

  // Calculate menu popularity for each demographic
  Popularity
  calculate_menu_popularity(const Restaurant& restaurant,
                            const core::food::PlayerRecipes& recipes) const;

  // Calculate furniture popularity for each demographic (without jukebox/TV)
  Popularity calculate_furniture_popularity(const Restaurant& restaurant) const;

  // Calculate furniture appeal including jukebox/TV bonuses
  // Unlike calculate_furniture_appeal(), this is const and returns values
  // rather than storing them. Used by Demographics screen.
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

  // Quality rating
  int quality_rating_{70};

  // percentage of customers turned away
  int rejection_rate_{0};

  // Competition data
  int demand_ratio_{0};
  int strongest_competitor_x_{0};
  int strongest_competitor_y_{0};
  int competitor_count_{0};
  int competition_score_{0};

  int jukebox_music_{0};
  int tv_channel_{0};
};
