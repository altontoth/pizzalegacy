/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ArgType.h"

// Forward declarations
class GameState;

namespace debug::command {

// Forward declarations
struct ParsedValue;

// Default value providers - type-safe sources for default values
enum class DefaultProvider {
  None,             // No default - argument is required
  ActivePlayer,     // state.active_player()->id()
  ActiveRestaurant, // state.active_restaurant()->id()
  ActiveCity,       // state.active_city_id()
  CurrentDate,      // state.time().current_date()
};

// Check if a default provider is compatible with an argument type
constexpr bool is_compatible(DefaultProvider provider, ArgType type) {
  switch (provider) {
  case DefaultProvider::None:
    return true; // None is always compatible (means no default)

  case DefaultProvider::ActivePlayer:
    return type == ArgType::PlayerId;

  case DefaultProvider::ActiveRestaurant:
    return type == ArgType::RestaurantId;

  case DefaultProvider::ActiveCity:
    return type == ArgType::CityId;

  case DefaultProvider::CurrentDate:
    return type == ArgType::Date;
  }
  return false;
}

// Resolve a default value from game state
// Defined in DefaultProvider.cpp to avoid circular dependencies
ParsedValue resolve_default(DefaultProvider provider, ArgType type,
                            GameState& state);

} // namespace debug::command
