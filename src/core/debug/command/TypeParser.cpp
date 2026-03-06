/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/command/TypeParser.h"

#include <algorithm>
#include <cctype>

namespace debug::command {

ParseResult TypeParser::parse(const std::string& token, ArgType type) {
  switch (type) {
  case ArgType::String:
    return parse_string(token);
  case ArgType::Int:
    return parse_int(token);
  case ArgType::Money:
    return parse_money(token);
  case ArgType::PlayerId:
    return parse_player_id(token);
  case ArgType::RestaurantId:
    return parse_restaurant_id(token);
  case ArgType::CityId:
    return parse_city_id(token);
  case ArgType::Bool:
    return parse_bool(token);
  case ArgType::ItemId:
    return parse_item_id(token);
  case ArgType::RecipeId:
    return parse_recipe_id(token);
  case ArgType::Date:
    return parse_date(token);
  }
  return ParseResult::failure("Unknown type");
}

ParseResult TypeParser::parse_string(const std::string& token) {
  ParsedValue val;
  val.type = ArgType::String;
  val.value = token;
  val.source = ValueSource::UserProvided;
  val.original_token = token;
  return ParseResult::success_result(std::move(val));
}

ParseResult TypeParser::parse_int(const std::string& token) {
  try {
    int64_t num = std::stoll(token);
    ParsedValue val;
    val.type = ArgType::Int;
    val.value = num;
    val.source = ValueSource::UserProvided;
    val.original_token = token;
    return ParseResult::success_result(std::move(val));
  }
  catch (const std::exception& e) {
    return ParseResult::failure(
        compat::format("Invalid integer: '{}' ({})", token, e.what()));
  }
}

ParseResult TypeParser::parse_money(const std::string& token) {
  try {
    // Parse as dollars, convert to cents
    int64_t dollars = std::stoll(token);
    Money money{dollars * 100};
    ParsedValue val;
    val.type = ArgType::Money;
    val.value = money;
    val.source = ValueSource::UserProvided;
    val.original_token = token;
    return ParseResult::success_result(std::move(val));
  }
  catch (const std::exception& e) {
    return ParseResult::failure(
        compat::format("Invalid money amount: '{}' ({})", token, e.what()));
  }
}

ParseResult TypeParser::parse_player_id(const std::string& token) {
  try {
    int32_t id = std::stoi(token);
    if (id < 0) {
      return ParseResult::failure(
          compat::format("Player ID must be non-negative, got: {}", id));
    }
    ParsedValue val;
    val.type = ArgType::PlayerId;
    val.value = core::PlayerId{id};
    val.source = ValueSource::UserProvided;
    val.original_token = token;
    return ParseResult::success_result(std::move(val));
  }
  catch (const std::exception& e) {
    return ParseResult::failure(
        compat::format("Invalid player ID: '{}' ({})", token, e.what()));
  }
}

ParseResult TypeParser::parse_restaurant_id(const std::string& token) {
  try {
    int32_t id = std::stoi(token);
    if (id < 0) {
      return ParseResult::failure(
          compat::format("Restaurant ID must be non-negative, got: {}", id));
    }
    ParsedValue val;
    val.type = ArgType::RestaurantId;
    val.value = core::RestaurantId{id};
    val.source = ValueSource::UserProvided;
    val.original_token = token;
    return ParseResult::success_result(std::move(val));
  }
  catch (const std::exception& e) {
    return ParseResult::failure(
        compat::format("Invalid restaurant ID: '{}' ({})", token, e.what()));
  }
}

ParseResult TypeParser::parse_city_id(const std::string& token) {
  try {
    int32_t id = std::stoi(token);
    if (id < 0) {
      return ParseResult::failure(
          compat::format("City ID must be non-negative, got: {}", id));
    }
    ParsedValue val;
    val.type = ArgType::CityId;
    val.value = core::CityId{id};
    val.source = ValueSource::UserProvided;
    val.original_token = token;
    return ParseResult::success_result(std::move(val));
  }
  catch (const std::exception& e) {
    return ParseResult::failure(
        compat::format("Invalid city ID: '{}' ({})", token, e.what()));
  }
}

ParseResult TypeParser::parse_bool(const std::string& token) {
  // Case-insensitive comparison
  std::string lower = token;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  bool value;
  if (lower == "true" || lower == "yes" || lower == "1") {
    value = true;
  }
  else if (lower == "false" || lower == "no" || lower == "0") {
    value = false;
  }
  else {
    return ParseResult::failure(compat::format(
        "Invalid boolean: '{}' (expected: true/false, yes/no, 1/0)", token));
  }

  ParsedValue val;
  val.type = ArgType::Bool;
  val.value = value;
  val.source = ValueSource::UserProvided;
  val.original_token = token;
  return ParseResult::success_result(std::move(val));
}

ParseResult TypeParser::parse_item_id(const std::string& token) {
  try {
    int id = std::stoi(token);
    if (id < 0 || id >= static_cast<int>(core::item::Item::Count)) {
      return ParseResult::failure(
          compat::format("Item ID out of range: {} (valid: 0-{})", id,
                         static_cast<int>(core::item::Item::Count) - 1));
    }
    ParsedValue val;
    val.type = ArgType::ItemId;
    val.value = static_cast<core::item::Item>(id);
    val.source = ValueSource::UserProvided;
    val.original_token = token;
    return ParseResult::success_result(std::move(val));
  }
  catch (const std::exception& e) {
    return ParseResult::failure(
        compat::format("Invalid item ID: '{}' ({})", token, e.what()));
  }
}

ParseResult TypeParser::parse_recipe_id(const std::string& token) {
  try {
    int32_t id = std::stoi(token);
    if (id < 0) {
      return ParseResult::failure(
          compat::format("Recipe ID must be non-negative, got: {}", id));
    }
    ParsedValue val;
    val.type = ArgType::RecipeId;
    val.value = core::RecipeId{id};
    val.source = ValueSource::UserProvided;
    val.original_token = token;
    return ParseResult::success_result(std::move(val));
  }
  catch (const std::exception& e) {
    return ParseResult::failure(
        compat::format("Invalid recipe ID: '{}' ({})", token, e.what()));
  }
}

ParseResult TypeParser::parse_date(const std::string& token) {
  // Simple implementation - parse as number of days from game start
  // A more robust implementation would parse "YYYY-MM-DD" format
  try {
    int32_t days_offset = std::stoi(token);
    if (days_offset < 0) {
      return ParseResult::failure(compat::format(
          "Date offset must be non-negative, got: {}", days_offset));
    }
    // Start from game start date (1995-03-01) and add days
    GameDate date = game_start_date.add_days(days_offset);
    ParsedValue val;
    val.type = ArgType::Date;
    val.value = date;
    val.source = ValueSource::UserProvided;
    val.original_token = token;
    return ParseResult::success_result(std::move(val));
  }
  catch (const std::exception& e) {
    return ParseResult::failure(
        compat::format("Invalid date: '{}' ({})", token, e.what()));
  }
}

} // namespace debug::command
