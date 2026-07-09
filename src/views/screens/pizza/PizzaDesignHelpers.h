/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

// Constants and helpers shared between the PizzaDesign translation units.

#include "core/food/PlayerRecipes.h"
#include "views/screens/restaurant/RecipeListRow.h"

#include <optional>
#include <variant>
#include <vector>

namespace pizza_design {

inline constexpr int CHOPPER_HEIGHT = 86;
inline constexpr int CHOPPER_WIDTH = 86;
inline constexpr int CHOPPER_OFFSET_X = 8;
inline constexpr int CHOPPER_OFFSET_Y = 256;

inline bool is_selected(const RecipeListRow& row,
                        const std::optional<RecipeKey>& active) {
  if (!active) {
    return false;
  }
  if (row.recipe_id && std::holds_alternative<core::RecipeId>(*active)) {
    return *row.recipe_id == std::get<core::RecipeId>(*active);
  }
  if (row.canonical &&
      std::holds_alternative<core::food::CanonicalPizza>(*active)) {
    return *row.canonical == std::get<core::food::CanonicalPizza>(*active);
  }
  return !row.recipe_id && !row.canonical &&
         std::holds_alternative<NewRowTag>(*active);
}

inline const RecipeListRow*
find_row_by_key(const std::vector<RecipeListRow>& rows, const RecipeKey& key) {
  for (const auto& r : rows) {
    if (std::holds_alternative<core::RecipeId>(key) && r.recipe_id &&
        *r.recipe_id == std::get<core::RecipeId>(key)) {
      return &r;
    }
    if (std::holds_alternative<core::food::CanonicalPizza>(key) &&
        r.canonical &&
        *r.canonical == std::get<core::food::CanonicalPizza>(key)) {
      return &r;
    }
    if (std::holds_alternative<NewRowTag>(key) && !r.recipe_id &&
        !r.canonical) {
      return &r; // "<NEW>"
    }
  }
  return nullptr;
}

} // namespace pizza_design
