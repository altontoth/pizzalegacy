/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace debug::command {

// Status of command execution
enum class CommandStatus {
  Success,          // Command completed successfully
  InvalidSyntax,    // Parsing failed (wrong number of args, etc.)
  ValidationFailed, // Validation failed (out of range, etc.)
  ExecutionError,   // Handler threw or reported error
  PartialSuccess,   // Some operations succeeded, some failed
};

// Rich result type for command execution
struct CommandResult {
  CommandStatus status = CommandStatus::Success;

  // Output lines (displayed to user)
  std::vector<std::string> messages;

  // Optional: structured output for machine parsing
  std::optional<std::string> json_output;

  // Error details (if failed)
  std::optional<std::string> error_message;
  std::optional<std::string> error_suggestion;

  // UI hints for console/UI integration
  struct UIHint {
    enum class Type { None, RefreshScreen, NavigateTo, ShowPopup };
    Type type = Type::None;
    std::string data; // Screen tag, popup message, etc.
  } ui_hint;

  // Factory methods for common cases
  static CommandResult success(std::vector<std::string> messages = {}) {
    CommandResult r;
    r.status = CommandStatus::Success;
    r.messages = std::move(messages);
    return r;
  }

  static CommandResult error(std::string error, std::string suggestion = "") {
    CommandResult r;
    r.status = CommandStatus::ExecutionError;
    r.error_message = std::move(error);
    r.error_suggestion = std::move(suggestion);
    return r;
  }

  static CommandResult invalid_syntax(std::string error,
                                      std::string suggestion = "") {
    CommandResult r;
    r.status = CommandStatus::InvalidSyntax;
    r.error_message = std::move(error);
    r.error_suggestion = std::move(suggestion);
    return r;
  }

  static CommandResult validation_failed(std::string error,
                                         std::string suggestion = "") {
    CommandResult r;
    r.status = CommandStatus::ValidationFailed;
    r.error_message = std::move(error);
    r.error_suggestion = std::move(suggestion);
    return r;
  }

  // Chainable builder methods
  CommandResult& add_message(const std::string& msg) {
    messages.push_back(msg);
    return *this;
  }

  CommandResult& with_ui_hint(UIHint::Type type, std::string data = "") {
    ui_hint.type = type;
    ui_hint.data = std::move(data);
    return *this;
  }

  CommandResult& with_json(std::string json) {
    json_output = std::move(json);
    return *this;
  }
};

} // namespace debug::command
