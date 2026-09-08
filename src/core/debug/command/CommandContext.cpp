/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/command/CommandContext.h"
#include <stdexcept>

namespace debug::command {

bool CommandContext::has_arg(size_t index) const {
  return index < args.size() && args[index].source != ValueSource::Absent;
}

const ParsedValue& CommandContext::arg(size_t index) const {
  if (!has_arg(index)) {
    throw std::runtime_error(compat::format(
        "Argument {} not provided (have {} arguments)", index, args.size()));
  }
  return args[index];
}

core::PlayerId CommandContext::player_id(size_t index) const {
  return arg(index).as_player_id();
}

core::RestaurantId CommandContext::restaurant_id(size_t index) const {
  return arg(index).as_restaurant_id();
}

core::CityId CommandContext::city_id(size_t index) const {
  return arg(index).as_city_id();
}

int64_t CommandContext::int_arg(size_t index) const {
  return arg(index).as_int();
}

std::string CommandContext::string_arg(size_t index) const {
  return arg(index).as_string();
}

Money CommandContext::money_arg(size_t index) const {
  return arg(index).as_money();
}

core::item::Item CommandContext::item_id(size_t index) const {
  return arg(index).as_item_id();
}

core::RecipeId CommandContext::recipe_id(size_t index) const {
  return arg(index).as_recipe_id();
}

bool CommandContext::bool_arg(size_t index) const {
  return arg(index).as_bool();
}

GameDate CommandContext::date_arg(size_t index) const {
  return arg(index).as_date();
}

} // namespace debug::command
