/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

// Constants and helpers shared between the PizzaDesign translation units.

#include "core/food/PlayerRecipes.h"
#include "core/util/LegacyConstants.h"
#include "views/screens/restaurant/RecipeListRow.h"

#include <optional>
#include <variant>
#include <vector>

namespace pizza_design {

inline constexpr int CHOPPER_HEIGHT = 86;
inline constexpr int CHOPPER_WIDTH = 86;
inline constexpr int CHOPPER_OFFSET_X = 8;
inline constexpr int CHOPPER_OFFSET_Y = 256;

// The pizza on the main panel
inline constexpr int PIZZA_CENTER_X =
    234 - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
inline constexpr int PIZZA_CENTER_Y =
    232 - LegacyConstants::WOOD_BORDER_HEIGHT_TOP;
inline constexpr int PIZZA_RADIUS = 122;

inline bool is_selected(const RecipeListRow& row,
                        const std::optional<RecipeKey>& active) {
  if (!active) {
    return false;
  }
  if (row.recipe_id && std::holds_alternative<core::RecipeId>(*active)) {
    return *row.recipe_id == std::get<core::RecipeId>(*active);
  }
  if (row.reference &&
      std::holds_alternative<core::food::StandardPizza>(*active)) {
    return *row.reference == std::get<core::food::StandardPizza>(*active);
  }
  return !row.recipe_id && !row.reference &&
         std::holds_alternative<NewRowTag>(*active);
}

inline const RecipeListRow*
find_row_by_key(const std::vector<RecipeListRow>& rows, const RecipeKey& key) {
  for (const auto& r : rows) {
    if (std::holds_alternative<core::RecipeId>(key) && r.recipe_id &&
        *r.recipe_id == std::get<core::RecipeId>(key)) {
      return &r;
    }
    if (std::holds_alternative<core::food::StandardPizza>(key) &&
        r.reference &&
        *r.reference == std::get<core::food::StandardPizza>(key)) {
      return &r;
    }
    if (std::holds_alternative<NewRowTag>(key) && !r.recipe_id &&
        !r.reference) {
      return &r; // "<NEW>"
    }
  }
  return nullptr;
}

} // namespace pizza_design
