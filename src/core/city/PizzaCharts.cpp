/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/PizzaCharts.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include "core/character/AiMenuSlot.h"
#include "core/character/Player.h"
#include "core/food/PlayerRecipes.h"
#include "core/food/Recipe.h"
#include "core/state/GameState.h"
#include "engine/Fatal.h"
#include "engine/Logger.h"
#include "resources/util/FileUtils.h"

namespace {

// An AI pizza id of 1 names MARKT.E line 51, so Pizza Napoletana on line 50 is
// out of reach of every id (bug in the original).
constexpr int ai_pizza_first_line = 51;
constexpr int ai_pizza_name_count = 99;
constexpr int ai_pizza_last_line =
    ai_pizza_first_line + ai_pizza_name_count - 1;

} // namespace

const std::vector<std::string>& PizzaCharts::ai_pizza_names() {
  static std::vector<std::string> names;
  if (!names.empty()) {
    return names;
  }

  const auto lines = fileutils::load_text_lines("t/markt.e");
  if (std::cmp_less(lines.size(), ai_pizza_last_line)) {
    fatal(compat::format("Not enough AI pizza names?! MARKT.E has {} lines, "
                         "expect at least {}",
                         lines.size(), ai_pizza_last_line));
  }

  for (int line_nr = ai_pizza_first_line; line_nr <= ai_pizza_last_line;
       ++line_nr) {
    std::string name = lines[static_cast<std::size_t>(line_nr - 1)];
    while (!name.empty() && name.back() == ' ') {
      name.pop_back();
    }
    names.push_back(std::move(name));
  }

  Logger::info("[PizzaCharts] Loaded {} AI pizza names from MARKT.E",
               names.size());
  return names;
}

std::string PizzaCharts::pizza_name(const Player& owner, PizzaChartRef pizza) {
  if (owner.is_human_controlled()) {
    const auto* recipe = owner.recipes().find(core::RecipeId{pizza.value});
    return recipe ? std::string(recipe->display_name()) : "Unknown Pizza";
  }
  const auto& names = ai_pizza_names();
  std::size_t idx = static_cast<std::size_t>(pizza.value) - 1;
  return idx < names.size() ? names[idx] : "Pizza Speciale";
}

void PizzaCharts::build(GameState& game_state) {
  struct Candidate {
    const Player* owner;
    PizzaChartRef pizza_ref;
    int score;
  };
  std::vector<Candidate> candidates;

  for (const auto* player : game_state.players()) {
    if (!player) {
      continue;
    }

    if (player->is_human_controlled()) {
      for (const auto* recipe : player->recipes().all()) {
        candidates.push_back(
            {player, PizzaChartRef::of(recipe->id()), recipe->chart_score()});
      }
    }
    else {
      for (const auto& slot : player->ai_menu_slots()) {
        if (slot.pizza_id != ai_pizza_none) {
          candidates.push_back(
              {player, PizzaChartRef::of(slot.pizza_id), slot.chart_score()});
        }
      }
    }
  }

  std::ranges::stable_sort(candidates, [](const auto& a, const auto& b) {
    return a.score > b.score;
  });

  std::vector<PizzaChartEntry> new_entries;
  int limit = std::min(static_cast<int>(candidates.size()), places);
  for (int i = 0; i < limit; ++i) {
    const auto& c = *candidates[i].owner;
    const PizzaChartRef ref = candidates[i].pizza_ref;

    // An owner can hold the same pizza in two menu slots, so yesterday's table
    // can name it twice; the lowest of those placings is the one reported.
    uint8_t prev_pos = 0;
    for (int j = 0; j < static_cast<int>(entries_.size()); ++j) {
      if (entries_[j].player_id == c.id() && entries_[j].pizza_ref == ref) {
        prev_pos = static_cast<uint8_t>(j + 1);
      }
    }

    new_entries.push_back({prev_pos, c.id(), ref, candidates[i].score});
  }

  entries_ = std::move(new_entries);
}
