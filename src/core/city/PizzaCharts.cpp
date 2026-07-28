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
#include "core/food/PlayerRecipes.h"
#include "core/food/Recipe.h"
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

namespace {

// A human's pizza is one of their recipes, an AI's a line of the canonical
// name list.
std::string pizza_name_of(const Player& owner, int pizza_ref) {
  if (owner.is_human_controlled()) {
    const auto* recipe = owner.recipes().find(core::RecipeId{pizza_ref});
    return recipe ? std::string(recipe->display_name()) : "Unknown Pizza";
  }
  const auto& names = PizzaCharts::ai_pizza_names();
  std::size_t idx = static_cast<std::size_t>(pizza_ref) - 1;
  return idx < names.size() ? names[idx] : "Pizza Speciale";
}

} // namespace

void PizzaCharts::build(GameState& game_state) {
  struct Candidate {
    const Player* owner;
    int pizza_ref;
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
            {player, recipe->id().value, recipe->chart_score()});
      }
    }
    else {
      for (const auto& slot : player->ai_menu_slots()) {
        if (slot.recipe_id != 0) {
          candidates.push_back({player, slot.recipe_id, slot.chart_score()});
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
    const int ref = candidates[i].pizza_ref;

    uint8_t prev_pos = 0;
    for (int j = 0; j < static_cast<int>(entries_.size()); ++j) {
      if (entries_[j].player_id == c.id() && entries_[j].pizza_ref == ref) {
        prev_pos = static_cast<uint8_t>(j + 1);
        break;
      }
    }

    new_entries.push_back({prev_pos, c.id(), ref, pizza_name_of(c, ref),
                           c.name(), candidates[i].score});
  }

  entries_ = std::move(new_entries);
}
