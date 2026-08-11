/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/ControlMap.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/city/City.h"
#include "core/city/EconomyTrend.h"
#include "core/city/Property.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"

namespace {

struct RestaurantInfo {
  core::PlayerId owner;
  int tile_x;
  int tile_y;
  int range;         // range_tiles + 1
  int threshold_sum; // sum of 6 competitive_strength values
};

} // namespace

void ControlMap::build(const GameState& game_state, const City& city) {
  // Collect all restaurants with their influence parameters
  std::vector<RestaurantInfo> restaurants;

  for (auto* player : game_state.players()) {
    if (!player) {
      continue;
    }

    for (auto* rest : game_state.restaurants_in_city(player->id(), city.id())) {
      int idx = rest->property().index();
      int tile_x = City::index_to_x(idx);
      int tile_y = City::index_to_y(idx);
      int range = rest->economy().range_tiles() + 1;

      // threshold_sum = sum of 6 demographic competitive_strength values
      const auto& cs = rest->economy().competitive_strength();
      int threshold_sum = 0;
      for (auto val : cs) {
        threshold_sum += val;
      }

      restaurants.push_back(
          {player->id(), tile_x, tile_y, range, threshold_sum});
    }
  }

  // Base threshold from economy trend, the factor is computed in floating point
  // and rounded to nearest before the << 6 scale.
  int economy = city.economy_trend().current();
  int base_threshold =
      static_cast<int>(std::lround(40.0 - (economy / 3.0))) * 64;

  std::array<int, NUMBER_OF_PLAYERS> cell_counts{};

  for (int cell_i = 0; cell_i < CONTROL_MAP_CELLS; ++cell_i) {
    int col = cell_i % NUMBER_OF_DISTRICT_CELL_COLUMNS;
    int row = cell_i / NUMBER_OF_DISTRICT_CELL_COLUMNS;
    int base_tile_x = col * TILES_PER_CELL;
    int base_tile_y = row * TILES_PER_CELL;

    // Start with the base threshold. A restaurant can only claim the cell by
    // exceeding it and every other restaurant.
    int best_score = base_threshold;
    std::optional<core::PlayerId> best_player;

    for (const auto& rest : restaurants) {
      int score = 0;

      // Sample 16 points in a 4x4 pattern (step by 2 within the 8x8 cell)
      for (int sy = 0; sy < 4; ++sy) {
        for (int sx = 0; sx < 4; ++sx) {
          int sample_x = base_tile_x + (sx * 2);
          int sample_y = base_tile_y + (sy * 2);
          int distance = std::abs(sample_x - rest.tile_x) +
                         std::abs(sample_y - rest.tile_y);

          // Influence reaches rest.range (range_tiles + 1) inclusive
          if (distance > rest.range) {
            continue;
          }

          int influence = std::max(0, rest.threshold_sum - (distance * 4));
          score += influence;
        }
      }

      if (score > best_score) {
        best_score = score;
        best_player = rest.owner;
      }
    }

    if (best_player.has_value()) {
      cell_owners_[cell_i] = best_player;
      cell_counts[best_player->value]++;
    }
    else {
      cell_owners_[cell_i] = std::nullopt;
    }
  }

  // Convert cell counts to percentages
  for (int p = 0; p < NUMBER_OF_PLAYERS; ++p) {
    player_percentages_[p] =
        (static_cast<float>(cell_counts[p]) * 100.0f) / CONTROL_MAP_CELLS;
  }
}

int ControlMap::cells_held(int player_index) const {
  return static_cast<int>(std::ranges::count_if(
      cell_owners_, [player_index](const std::optional<core::PlayerId>& owner) {
        return owner && owner->value == player_index;
      }));
}

std::optional<core::PlayerId> ControlMap::market_leader() const {
  int best = 0;
  std::optional<core::PlayerId> leader;
  for (int p = 0; p < NUMBER_OF_PLAYERS; ++p) {
    const int held = cells_held(p);
    if (held > best) {
      best = held;
      leader = core::PlayerId{p};
    }
  }
  return leader;
}
