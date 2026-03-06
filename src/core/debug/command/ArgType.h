/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string_view>

namespace debug::command {

// Supported argument types for command parsing
enum class ArgType {
  String,       // Raw string
  Int,          // Integer (64-bit)
  Money,        // Dollar amount (converted to Money type)
  PlayerId,     // Player ID with '@' support for active player
  RestaurantId, // Restaurant ID with '@' support for active restaurant
  CityId,       // City ID with '@' support for active city
  Bool,         // Boolean (true/false, yes/no, 1/0)
  ItemId,       // Inventory item ID
  RecipeId,     // Recipe/Pizza ID
  Date,         // Game date
};

// Get human-readable type name for error messages
constexpr std::string_view type_name(ArgType type) {
  switch (type) {
  case ArgType::String:
    return "string";
  case ArgType::Int:
    return "integer";
  case ArgType::Money:
    return "money amount";
  case ArgType::PlayerId:
    return "player ID";
  case ArgType::RestaurantId:
    return "restaurant ID";
  case ArgType::CityId:
    return "city ID";
  case ArgType::Bool:
    return "boolean";
  case ArgType::ItemId:
    return "item ID";
  case ArgType::RecipeId:
    return "recipe ID";
  case ArgType::Date:
    return "date";
  }
  return "unknown";
}

} // namespace debug::command
