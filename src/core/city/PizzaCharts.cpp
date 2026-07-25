/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/PizzaCharts.h"

#include <algorithm>
#include <string>
#include <vector>
#include "core/character/AiMenuSlot.h"
#include "core/character/Player.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "engine/Fatal.h"
#include "engine/Logger.h"
#include "resources/util/FileUtils.h"

const std::vector<std::string>& PizzaCharts::ai_pizza_names() {
  static std::vector<std::string> names;
  if (!names.empty()) {
    return names;
  }

  auto file = fileutils::open("t/markt.e", std::ios::in);

  std::string line;
  int line_nr = 0;
  while (std::getline(file, line)) {
    ++line_nr;
    if (line_nr >= 51 && line_nr <= 150) {
      // Trim trailing whitespace/carriage returns
      while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
        line.pop_back();
      }
      names.push_back(line);
    }
    if (line_nr >= 150) {
      break;
    }
  }

  // Pad to 100 if file was short
  if (names.size() < 99) {
    fatal(compat::format("Not enough AI pizza names?! I have {}, expect 99",
                         names.size()));
  }

  Logger::info("[PizzaCharts] Loaded {} AI pizza names from MARKT.E",
               names.size());
  return names;
}

void PizzaCharts::build(GameState& game_state) {
  // Collect scored entries from all players
  struct ScoredEntry {
    core::PlayerId player_id;
    std::string pizza_name;
    std::string player_name;
    int score;
  };
  std::vector<ScoredEntry> candidates;

  auto players = game_state.players();
  for (auto* player : players) {
    if (!player) {
      continue;
    }

    if (player->is_human_controlled()) {
      // The chart is one global top-10 across every city, matching the
      // original's single table: humans contribute per restaurant (all cities),
      // AI per menu slot. Only the per-City previous-position diff makes the
      // rendered arrows differ between cities.
      std::unordered_map<core::RecipeId, int, core::RecipeId::Hasher>
          sales_by_recipe;

      for (auto* restaurant : game_state.restaurants_for(player->id())) {
        if (!restaurant) {
          continue;
        }
        for (const auto& day : restaurant->stats().last_n_days(7)) {
          for (const auto& [recipe_id, sales_data] : day.pizza_sales) {
            sales_by_recipe[recipe_id] += sales_data.count_sold;
          }
        }
      }

      for (const auto& [recipe_id, total_sold] : sales_by_recipe) {
        if (total_sold <= 0) {
          continue;
        }
        const auto* recipe = player->recipes().find(recipe_id);
        std::string name =
            recipe ? std::string(recipe->display_name()) : "Unknown Pizza";
        candidates.push_back(
            {player->id(), std::move(name), player->name(), total_sold});
      }
    }
    else {
      // AI: score from menu slot history buffer
      const auto& names = ai_pizza_names();
      for (const auto& slot : player->ai_menu_slots()) {
        if (slot.recipe_id == 0) {
          continue;
        }
        int score = slot.chart_score();
        if (score <= 0) {
          continue;
        }
        int name_idx = slot.recipe_id - 1;
        std::string name =
            (name_idx >= 0 && name_idx < static_cast<int>(names.size()))
                ? names[name_idx]
                : "Pizza Speciale";
        candidates.push_back(
            {player->id(), std::move(name), player->name(), score});
      }
    }
  }

  // Sort descending by score, take top 10
  std::ranges::sort(candidates, [](const auto& a, const auto& b) {
    return a.score > b.score;
  });

  std::vector<PizzaChartEntry> new_entries;
  int limit = std::min(static_cast<int>(candidates.size()), 10);
  for (int i = 0; i < limit; ++i) {
    const auto& c = candidates[i];

    // Match against current chart (about to become previous) for
    // previous_position
    uint8_t prev_pos = 0;
    for (int j = 0; j < static_cast<int>(entries_.size()); ++j) {
      if (entries_[j].player_id == c.player_id &&
          entries_[j].pizza_name == c.pizza_name) {
        prev_pos = static_cast<uint8_t>(j + 1);
        break;
      }
    }

    new_entries.push_back(
        {prev_pos, c.player_id, c.pizza_name, c.player_name, c.score});
  }

  previous_ = std::move(entries_);
  entries_ = std::move(new_entries);
}
