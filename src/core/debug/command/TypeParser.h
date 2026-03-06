/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>

#include "ArgType.h"
#include "ParsedValue.h"

namespace debug::command {

// Result of type parsing (string -> typed value)
struct ParseResult {
  bool success;
  std::optional<ParsedValue> value;
  std::string error;

  static ParseResult success_result(ParsedValue val) {
    return ParseResult{true, std::move(val), ""};
  }

  static ParseResult failure(std::string error) {
    return ParseResult{false, std::nullopt, std::move(error)};
  }
};

// Type parser - converts strings to typed values
class TypeParser {
public:
  // Main entry point
  static ParseResult parse(const std::string& token, ArgType type);

private:
  // Type-specific parsers
  static ParseResult parse_string(const std::string& token);
  static ParseResult parse_int(const std::string& token);
  static ParseResult parse_money(const std::string& token);
  static ParseResult parse_player_id(const std::string& token);
  static ParseResult parse_restaurant_id(const std::string& token);
  static ParseResult parse_city_id(const std::string& token);
  static ParseResult parse_bool(const std::string& token);
  static ParseResult parse_item_id(const std::string& token);
  static ParseResult parse_recipe_id(const std::string& token);
  static ParseResult parse_date(const std::string& token);
};

} // namespace debug::command
