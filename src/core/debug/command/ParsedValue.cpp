/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/command/ParsedValue.h"
#include <magic_enum/magic_enum.hpp>
#include <stdexcept>

namespace debug::command {

std::string ParsedValue::as_string() const {
  if (type != ArgType::String) {
    throw std::runtime_error(
        compat::format("Expected string, got {}", type_name(type)));
  }
  return std::get<std::string>(value);
}

int64_t ParsedValue::as_int() const {
  if (type != ArgType::Int) {
    throw std::runtime_error(
        compat::format("Expected integer, got {}", type_name(type)));
  }
  return std::get<int64_t>(value);
}

Money ParsedValue::as_money() const {
  if (type != ArgType::Money) {
    throw std::runtime_error(
        compat::format("Expected money, got {}", type_name(type)));
  }
  return std::get<Money>(value);
}

core::PlayerId ParsedValue::as_player_id() const {
  if (type != ArgType::PlayerId) {
    throw std::runtime_error(
        compat::format("Expected player ID, got {}", type_name(type)));
  }
  return std::get<core::PlayerId>(value);
}

core::RestaurantId ParsedValue::as_restaurant_id() const {
  if (type != ArgType::RestaurantId) {
    throw std::runtime_error(
        compat::format("Expected restaurant ID, got {}", type_name(type)));
  }
  return std::get<core::RestaurantId>(value);
}

core::CityId ParsedValue::as_city_id() const {
  if (type != ArgType::CityId) {
    throw std::runtime_error(
        compat::format("Expected city ID, got {}", type_name(type)));
  }
  return std::get<core::CityId>(value);
}

bool ParsedValue::as_bool() const {
  if (type != ArgType::Bool) {
    throw std::runtime_error(
        compat::format("Expected boolean, got {}", type_name(type)));
  }
  return std::get<bool>(value);
}

core::item::Item ParsedValue::as_item_id() const {
  if (type != ArgType::ItemId) {
    throw std::runtime_error(
        compat::format("Expected item ID, got {}", type_name(type)));
  }
  return std::get<core::item::Item>(value);
}

core::RecipeId ParsedValue::as_recipe_id() const {
  if (type != ArgType::RecipeId) {
    throw std::runtime_error(
        compat::format("Expected recipe ID, got {}", type_name(type)));
  }
  return std::get<core::RecipeId>(value);
}

GameDate ParsedValue::as_date() const {
  if (type != ArgType::Date) {
    throw std::runtime_error(
        compat::format("Expected date, got {}", type_name(type)));
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
  case ValueSource::Absent:
    return "not given";
  }
  return "unknown";
}

ParsedValue ParsedValue::absent(ArgType type) {
  ParsedValue val;
  val.type = type;
  val.source = ValueSource::Absent;
  return val;
}

} // namespace debug::command
