/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// views/screens/recipe_list_rows.h
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/food/PlayerRecipes.h"
#include "core/food/StandardPizza.h"

struct RecipeListRow {
  std::string name;
  // If this recipe has been created:
  std::optional<core::RecipeId> recipe_id = std::nullopt;
  // If this is a standard pizza row:
  std::optional<core::food::StandardPizza> reference = std::nullopt;
  // How many restaurants have this recipe on their menu
  int listed_count{0};
};

// Count how many restaurants have this recipe on the menu
template <class Restaurants>
int listed_count_for(const Restaurants& restaurants, core::RecipeId id) {
  int count = 0;
  for (const auto* r : restaurants) {
    bool present = false;
    auto menu = r->menu();
    for (const auto& e : menu.entries()) {
      if (e.recipe_id == id) {
        present = true;
        break;
      }
    }
    if (present) {
      ++count;
    }
  }
  return count;
}

// Build rows: all standard pizzas in enum order, then all user recipes.
template <class Restaurants>
std::vector<RecipeListRow>
build_recipe_list_rows(const core::food::PlayerRecipes& pr,
                       const Restaurants& restaurants) {
  using core::food::StandardPizza;
  using core::food::to_index;

  std::vector<RecipeListRow> rows;
  rows.reserve(core::food::STANDARD_PIZZA_COUNT + pr.user.size());

  // Standard rows
  for (int i = 0; i < core::food::STANDARD_PIZZA_COUNT; ++i) {
    const auto cid = static_cast<StandardPizza>(i);
    const auto name_sv = core::food::to_string(cid);
    const auto& slot = pr.standard[i];

    if (slot.recipe) {
      const auto& recipe = *slot.recipe;
      const int cnt = listed_count_for(restaurants, recipe.id());
      rows.push_back(RecipeListRow{
          .name = std::string(recipe.display_name()), // allow UI to own string
          .recipe_id = recipe.id(),
          .reference = cid,
          .listed_count = cnt});
    }
    else {
      rows.push_back(RecipeListRow{.name = std::string(name_sv),
                                   .recipe_id = std::nullopt, // not baked yet
                                   .reference = cid,
                                   .listed_count = 0}); // won't render anyway
    }
  }

  // User rows (in creation order)
  for (const auto& recipe : pr.user) {
    const int cnt = listed_count_for(restaurants, recipe.id());
    rows.push_back(RecipeListRow{.name = std::string(recipe.display_name()),
                                 .recipe_id = recipe.id(),
                                 .reference = std::nullopt,
                                 .listed_count = cnt});
  }

  return rows;
}
