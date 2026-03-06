/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "ArgType.h"
#include "DefaultProvider.h"

// Forward declarations
class GameState;

namespace debug::command {

struct ParsedValue;

// Validation result - returned by validators
struct ValidationResult {
  bool valid;
  std::string error;
  std::string suggestion;

  static ValidationResult success() {
    return ValidationResult{.valid = true, .error = "", .suggestion = ""};
  }

  static ValidationResult failure(std::string error,
                                  std::string suggestion = "") {
    return ValidationResult{.valid = false,
                            .error = std::move(error),
                            .suggestion = std::move(suggestion)};
  }
};

// Custom validator function type
using CustomValidator =
    std::function<ValidationResult(const ParsedValue&, GameState&)>;

// Specification for a single command argument
struct ArgSpec {
  std::string name;                 // Argument name (for help text)
  ArgType type;                     // Expected type
  bool optional;                    // Can this argument be omitted?
  DefaultProvider default_provider; // How to get default value
  std::string description;          // Help text

  // Validation constraints (applied after parsing)
  std::optional<int64_t> min_value;
  std::optional<int64_t> max_value;
  std::optional<std::vector<std::string>> allowed_values;

  // Custom validator for complex logic
  std::optional<CustomValidator> custom_validator;

  // Factory methods for common cases
  static ArgSpec required(std::string name, ArgType type, std::string desc) {
    return ArgSpec{
        .name = std::move(name),
        .type = type,
        .optional = false,
        .default_provider = DefaultProvider::None,
        .description = std::move(desc),
        .min_value = std::nullopt,
        .max_value = std::nullopt,
        .allowed_values = std::nullopt,
        .custom_validator = std::nullopt,
    };
  }

  static ArgSpec optional_with_default(std::string name, ArgType type,
                                       DefaultProvider provider,
                                       std::string desc) {
    return ArgSpec{
        .name = std::move(name),
        .type = type,
        .optional = true,
        .default_provider = provider,
        .description = std::move(desc),
        .min_value = std::nullopt,
        .max_value = std::nullopt,
        .allowed_values = std::nullopt,
        .custom_validator = std::nullopt,
    };
  }
};

} // namespace debug::command
