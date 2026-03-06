/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <vector>
#include "ParsedValue.h"

// Forward declarations
class GameState;
class GameContext;

namespace debug::command {

// Context passed to command handlers
struct CommandContext {
  GameState& state;
  GameContext& context;
  std::vector<ParsedValue> args; // Validated positional arguments

  // Access argument with bounds checking
  const ParsedValue& arg(size_t index) const;

  // Type-safe convenience accessors
  core::PlayerId player_id(size_t index = 0) const;
  core::RestaurantId restaurant_id(size_t index) const;
  core::CityId city_id(size_t index) const;
  int64_t int_arg(size_t index) const;
  std::string string_arg(size_t index) const;
  Money money_arg(size_t index) const;
  core::item::Item item_id(size_t index) const;
  core::RecipeId recipe_id(size_t index) const;
  bool bool_arg(size_t index) const;
  GameDate date_arg(size_t index) const;
};

} // namespace debug::command
