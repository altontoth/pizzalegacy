/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/command/ValueValidator.h"

#include <algorithm>
#include "core/state/GameState.h"

namespace debug::command {

ValidationResult ValueValidator::validate_spec_constraints(
    const ParsedValue& value, const ArgSpec& spec, GameState& state) {
  // Range validation
  if (spec.min_value || spec.max_value) {
    auto range_result = validate_range(value, spec.min_value, spec.max_value);
    if (!range_result.valid) {
      return range_result;
    }
  }

  // Allowed values
  if (spec.allowed_values) {
    auto allowed_result = validate_allowed_values(value, *spec.allowed_values);
    if (!allowed_result.valid) {
      return allowed_result;
    }
  }

  // Custom validator
  if (spec.custom_validator) {
    auto custom_result = (*spec.custom_validator)(value, state);
    if (!custom_result.valid) {
      return custom_result;
    }
  }

  return ValidationResult::success();
}

ValidationResult ValueValidator::validate(const ParsedValue& value,
                                          const ArgSpec& spec,
                                          GameState& state) {
  auto spec_result = validate_spec_constraints(value, spec, state);
  if (!spec_result.valid) {
    return spec_result;
  }

  // Type-specific existence checks
  switch (value.type) {
  case ArgType::PlayerId: {
    auto result = validate_player_exists(value.as_player_id(), state);
    if (!result.valid) {
      return result;
    }
    break;
  }
  case ArgType::RestaurantId: {
    auto result = validate_restaurant_exists(value.as_restaurant_id(), state);
    if (!result.valid) {
      return result;
    }
    break;
  }
  case ArgType::CityId: {
    auto result = validate_city_exists(value.as_city_id(), state);
    if (!result.valid) {
      return result;
    }
    break;
  }
  default:
    // No existence check needed for other types
    break;
  }

  return ValidationResult::success();
}

ValidationResult ValueValidator::validate_range(const ParsedValue& value,
                                                std::optional<int64_t> min,
                                                std::optional<int64_t> max) {
  int64_t num_value;

  // Extract numeric value based on type
  switch (value.type) {
  case ArgType::Int:
    num_value = value.as_int();
    break;
  case ArgType::Money:
    num_value = value.as_money().cents;
    break;
  case ArgType::PlayerId:
    num_value = value.as_player_id().value;
    break;
  case ArgType::RestaurantId:
    num_value = value.as_restaurant_id().value;
    break;
  case ArgType::CityId:
    num_value = value.as_city_id().value;
    break;
  case ArgType::ItemId:
    num_value = static_cast<int64_t>(value.as_item_id());
    break;
  case ArgType::RecipeId:
    num_value = value.as_recipe_id().value;
    break;
  default:
    return ValidationResult::success(); // Not a numeric type
  }

  // Check range
  if (min && num_value < *min) {
    return ValidationResult::failure(
        compat::format("Value {} is below minimum {}", num_value, *min),
        compat::format("Valid range: {}-{}", *min, max.value_or(INT64_MAX)));
  }

  if (max && num_value > *max) {
    return ValidationResult::failure(
        compat::format("Value {} exceeds maximum {}", num_value, *max),
        compat::format("Valid range: {}-{}", min.value_or(0), *max));
  }

  return ValidationResult::success();
}

ValidationResult ValueValidator::validate_allowed_values(
    const ParsedValue& value, const std::vector<std::string>& allowed) {
  std::string str_value;

  // Convert value to string for comparison
  switch (value.type) {
  case ArgType::String:
    str_value = value.as_string();
    break;
  case ArgType::Int:
    str_value = std::to_string(value.as_int());
    break;
  default:
    // For other types, use original token
    str_value = value.original_token;
    break;
  }

  // Check if value is in allowed list
  auto it = std::ranges::find(allowed, str_value);
  if (it != allowed.end()) {
    return ValidationResult::success();
  }

  // Not found - list all allowed values
  std::string error = compat::format("'{}' is not a valid value", str_value);
  std::string suggest_text = "Allowed values: ";
  for (size_t i = 0; i < allowed.size(); ++i) {
    if (i > 0) {
      suggest_text += ", ";
    }
    suggest_text += allowed[i];
  }

  return ValidationResult::failure(error, suggest_text);
}

ValidationResult ValueValidator::validate_player_exists(core::PlayerId id,
                                                        GameState& state) {
  auto* player = state.player(id);
  if (!player) {
    return ValidationResult::failure(
        compat::format("Player {} does not exist", id.value),
        "Valid player IDs: 0-7");
  }
  return ValidationResult::success();
}

ValidationResult
ValueValidator::validate_restaurant_exists(core::RestaurantId id,
                                           GameState& state) {
  auto* restaurant = state.restaurant(id);
  if (!restaurant) {
    return ValidationResult::failure(
        compat::format("Restaurant {} does not exist", id.value),
        "Use valid restaurant ID or @ for active restaurant");
  }
  return ValidationResult::success();
}

ValidationResult ValueValidator::validate_city_exists(core::CityId id,
                                                      GameState& /*state*/) {
  // Cities are created on-demand when accessed, so we only validate that the
  // ID is in the valid range (0-9). The city will be created if needed when
  // the command handler runs.
  if (id.value < 0 || id.value >= NUMBER_OF_CITIES) {
    return ValidationResult::failure(
        compat::format("City ID {} is out of range", id.value),
        "Valid city IDs: 0-9");
  }
  return ValidationResult::success();
}

} // namespace debug::command
