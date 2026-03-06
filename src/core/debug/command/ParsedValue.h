/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <variant>

#include "ArgType.h"
#include "DefaultProvider.h"
#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/item/Item.h"
#include "core/time/GameTime.h"

namespace debug::command {

// Where did this value come from?
enum class ValueSource {
  UserProvided,    // Explicit value from command line
  DefaultProvider, // Resolved from a DefaultProvider
};

// A parsed and typed command argument with provenance tracking
struct ParsedValue {
  ArgType type;

  // The actual typed value
  std::variant<std::string,        // String
               int64_t,            // Int
               Money,              // Money
               core::PlayerId,     // PlayerId
               core::RestaurantId, // RestaurantId
               core::CityId,       // CityId
               bool,               // Bool
               core::item::Item,   // ItemId
               core::RecipeId,     // RecipeId
               GameDate            // Date
               >
      value;

  // Provenance information
  ValueSource source;
  std::string original_token; // Raw input string (for error messages)
  std::optional<DefaultProvider> provider_used; // If source == DefaultProvider

  // Type-safe accessors (throw std::bad_variant_access if wrong type)
  std::string as_string() const;
  int64_t as_int() const;
  Money as_money() const;
  core::PlayerId as_player_id() const;
  core::RestaurantId as_restaurant_id() const;
  core::CityId as_city_id() const;
  bool as_bool() const;
  core::item::Item as_item_id() const;
  core::RecipeId as_recipe_id() const;
  GameDate as_date() const;

  // For logging/debugging - describes where this value came from
  std::string describe_source() const;
};

} // namespace debug::command
