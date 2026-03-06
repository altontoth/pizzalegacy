/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/command/ParsedValue.h"
#include <magic_enum/magic_enum.hpp>
#include "engine/Fatal.h"

namespace debug::command {

std::string ParsedValue::as_string() const {
  if (type != ArgType::String) {
    fatal(compat::format("Expected string, got {}", type_name(type)));
  }
  return std::get<std::string>(value);
}

int64_t ParsedValue::as_int() const {
  if (type != ArgType::Int) {
    fatal(compat::format("Expected integer, got {}", type_name(type)));
  }
  return std::get<int64_t>(value);
}

Money ParsedValue::as_money() const {
  if (type != ArgType::Money) {
    fatal(compat::format("Expected money, got {}", type_name(type)));
  }
  return std::get<Money>(value);
}

core::PlayerId ParsedValue::as_player_id() const {
  if (type != ArgType::PlayerId) {
    fatal(compat::format("Expected player ID, got {}", type_name(type)));
  }
  return std::get<core::PlayerId>(value);
}

core::RestaurantId ParsedValue::as_restaurant_id() const {
  if (type != ArgType::RestaurantId) {
    fatal(compat::format("Expected restaurant ID, got {}", type_name(type)));
  }
  return std::get<core::RestaurantId>(value);
}

core::CityId ParsedValue::as_city_id() const {
  if (type != ArgType::CityId) {
    fatal(compat::format("Expected city ID, got {}", type_name(type)));
  }
  return std::get<core::CityId>(value);
}

bool ParsedValue::as_bool() const {
  if (type != ArgType::Bool) {
    fatal(compat::format("Expected boolean, got {}", type_name(type)));
  }
  return std::get<bool>(value);
}

core::item::Item ParsedValue::as_item_id() const {
  if (type != ArgType::ItemId) {
    fatal(compat::format("Expected item ID, got {}", type_name(type)));
  }
  return std::get<core::item::Item>(value);
}

core::RecipeId ParsedValue::as_recipe_id() const {
  if (type != ArgType::RecipeId) {
    fatal(compat::format("Expected recipe ID, got {}", type_name(type)));
  }
  return std::get<core::RecipeId>(value);
}

GameDate ParsedValue::as_date() const {
  if (type != ArgType::Date) {
    fatal(compat::format("Expected date, got {}", type_name(type)));
  }
  return std::get<GameDate>(value);
}

std::string ParsedValue::describe_source() const {
  switch (source) {
  case ValueSource::UserProvided:
    return compat::format("'{}'", original_token);
  case ValueSource::DefaultProvider:
    if (provider_used) {
      return compat::format("default ({})",
                            magic_enum::enum_name(*provider_used));
    }
    return "default";
  }
  return "unknown";
}

} // namespace debug::command
