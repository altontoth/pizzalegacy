/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ArgSpec.h"
#include "ParsedValue.h"

// Forward declaration
class GameState;

namespace debug::command {

// Value validator - validates parsed values against constraints
class ValueValidator {
public:
  // Main validation entry point
  static ValidationResult validate(const ParsedValue& value,
                                   const ArgSpec& spec, GameState& state);

  // Only validate constraints declared by the ArgSpec. Defaults skip existence
  // checks because some commands use them for things they are about to create.
  static ValidationResult validate_spec_constraints(const ParsedValue& value,
                                                    const ArgSpec& spec,
                                                    GameState& state);

private:
  // Specific validators
  static ValidationResult validate_range(const ParsedValue& value,
                                         std::optional<int64_t> min,
                                         std::optional<int64_t> max);

  static ValidationResult
  validate_allowed_values(const ParsedValue& value,
                          const std::vector<std::string>& allowed);

  static ValidationResult validate_player_exists(core::PlayerId id,
                                                 GameState& state);

  static ValidationResult validate_restaurant_exists(core::RestaurantId id,
                                                     GameState& state);

  static ValidationResult validate_city_exists(core::CityId id,
                                               GameState& state);
};

} // namespace debug::command
