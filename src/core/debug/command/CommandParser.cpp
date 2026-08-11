/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/command/CommandParser.h"
#include "DefaultProvider.h"
#include "TypeParser.h"
#include "ValueValidator.h"
#include "core/state/GameState.h"

namespace debug::command {

CommandParser::ParsedCommand
CommandParser::parse(const CommandDef& root,
                     const std::vector<std::string>& tokens, GameState& state) {
  ParsedCommand result;
  result.success = false;

  // Empty command
  if (tokens.empty()) {
    result.error_result = CommandResult::invalid_syntax(
        "No command specified", "Try 'help' for a list of commands");
    return result;
  }

  // Find the command in the tree
  auto match = find_command(root, tokens);
  if (!match.command) {
    result.error_result = CommandResult::invalid_syntax(
        compat::format("Unknown command: '{}'", tokens[0]),
        "Try 'help' for a list of commands");
    return result;
  }

  result.command = match.command;

  // Check if this is a leaf command (has handler)
  if (!match.command->is_leaf()) {
    // Build suggestion with available subcommands
    std::string subcommands;
    for (const auto& [name, _] : match.command->subcommands) {
      if (!subcommands.empty()) {
        subcommands += ", ";
      }
      subcommands += name;
    }
    std::string suggestion =
        subcommands.empty()
            ? compat::format("Try 'help {}' for usage", match.command->name)
            : compat::format("Available subcommands: {}", subcommands);

    result.error_result = CommandResult::invalid_syntax(
        compat::format("'{}' is not a complete command", match.command->name),
        suggestion);
    return result;
  }

  // Extract argument tokens (everything after the command path)
  std::vector<std::string> arg_tokens(tokens.begin() + match.tokens_consumed,
                                      tokens.end());

  // Parse arguments
  const auto& arg_specs = match.command->args;
  std::vector<ParsedValue> parsed_args;

  for (size_t i = 0; i < arg_specs.size(); ++i) {
    const auto& spec = arg_specs[i];

    // Check if we have a token for this argument
    if (i < arg_tokens.size()) {
      // Parse the provided token
      auto parsed = parse_argument(arg_tokens[i], spec, state, result);
      if (!parsed) {
        // Parsing failed - error set in result by parse_argument
        return result;
      }
      parsed_args.push_back(std::move(*parsed));
    }
    else {
      // No token provided - check if optional with default
      if (spec.optional && spec.default_provider != DefaultProvider::None) {
        // Resolve default
        try {
          auto default_val =
              resolve_default(spec.default_provider, spec.type, state);
          auto validation = ValueValidator::validate_spec_constraints(
              default_val, spec, state);
          if (!validation.valid) {
            result.error_result = CommandResult::error(
                compat::format("Default for argument '{}' is not usable: {}",
                               spec.name, validation.error));
            return result;
          }
          parsed_args.push_back(std::move(default_val));
        }
        catch (const std::exception& e) {
          result.error_result = CommandResult::error(
              compat::format("Failed to resolve default for argument '{}': {}",
                             spec.name, e.what()));
          return result;
        }
      }
      else if (!spec.optional) {
        // Required argument missing
        result.error_result =
            missing_argument_error(*match.command, spec, arg_tokens.size());
        return result;
      }
      // else: optional with no default - skip it
    }
  }

  // Check for too many arguments
  if (arg_tokens.size() > arg_specs.size()) {
    result.error_result = too_many_arguments_error(
        *match.command, arg_specs.size(), arg_tokens.size());
    return result;
  }

  // All arguments parsed successfully
  result.args = std::move(parsed_args);
  result.success = true;
  return result;
}

CommandParser::CommandMatch
CommandParser::find_command(const CommandDef& root,
                            const std::vector<std::string>& tokens) {
  const CommandDef* current = &root;
  size_t consumed = 0;

  // Special case: if root matches the first token, start there
  if (!tokens.empty() && tokens[0] == root.name) {
    consumed = 1;
  }
  else {
    // Check if first token matches any root alias
    bool found_alias = false;
    for (const auto& alias : root.aliases) {
      if (tokens[0] == alias) {
        consumed = 1;
        found_alias = true;
        break;
      }
    }
    if (!found_alias) {
      return CommandMatch{nullptr, 0};
    }
  }

  // Walk subcommands
  while (consumed < tokens.size()) {
    const std::string& token = tokens[consumed];

    // Try to find subcommand by name
    auto it = current->subcommands.find(token);
    if (it != current->subcommands.end()) {
      current = &it->second;
      ++consumed;
    }
    else {
      // Try to find subcommand by alias
      bool found_alias = false;
      for (const auto& [name, subcmd] : current->subcommands) {
        for (const auto& alias : subcmd.aliases) {
          if (alias == token) {
            current = &subcmd;
            ++consumed;
            found_alias = true;
            break;
          }
        }
        if (found_alias) {
          break;
        }
      }

      if (!found_alias) {
        // No more subcommands match - stop here
        break;
      }
    }
  }

  return CommandMatch{current, consumed};
}

std::optional<ParsedValue>
CommandParser::parse_argument(const std::string& token, const ArgSpec& spec,
                              GameState& state, ParsedCommand& result) {
  // Check if this is a default marker
  if (is_default_marker(token)) {
    if (spec.default_provider != DefaultProvider::None) {
      try {
        auto default_val =
            resolve_default(spec.default_provider, spec.type, state);
        auto validation =
            ValueValidator::validate_spec_constraints(default_val, spec, state);
        if (!validation.valid) {
          result.error_result = CommandResult::error(
              compat::format("Default for argument '{}' is not usable: {}",
                             spec.name, validation.error));
          return std::nullopt;
        }
        return default_val;
      }
      catch (const std::exception& e) {
        result.error_result = CommandResult::error(compat::format(
            "Failed to resolve default for '{}': {}", spec.name, e.what()));
        return std::nullopt;
      }
    }
    else {
      result.error_result = CommandResult::invalid_syntax(
          compat::format("'@' cannot be used for argument '{}'", spec.name),
          "This argument has no default value");
      return std::nullopt;
    }
  }

  // Parse the token to the expected type
  auto parse_result = TypeParser::parse(token, spec.type);
  if (!parse_result.success) {
    result.error_result = CommandResult::invalid_syntax(
        compat::format("Invalid value '{}' for argument '{}'", token,
                       spec.name),
        compat::format("Expected {}", type_name(spec.type)));
    return std::nullopt;
  }

  // Validate the parsed value
  auto validation_result =
      ValueValidator::validate(*parse_result.value, spec, state);
  if (!validation_result.valid) {
    result.error_result = CommandResult::invalid_syntax(
        validation_result.error, validation_result.suggestion);
    return std::nullopt;
  }

  return parse_result.value;
}

bool CommandParser::is_default_marker(const std::string& token) const {
  return token == "@";
}

CommandResult CommandParser::missing_argument_error(const CommandDef& cmd,
                                                    const ArgSpec& spec,
                                                    size_t provided) {
  std::string error =
      compat::format("Missing required argument: {}", spec.name);
  std::string suggestion = build_usage_string(cmd);

  // Add note about which arguments were provided vs needed
  if (provided > 0) {
    suggestion +=
        compat::format("\n({} argument(s) provided, more required)", provided);
  }

  return CommandResult::invalid_syntax(error, suggestion);
}

std::string CommandParser::build_usage_string(const CommandDef& cmd) const {
  std::string usage = compat::format("Usage: {}", cmd.name);

  for (const auto& arg : cmd.args) {
    if (arg.optional) {
      // Show optional args with default indicator
      if (arg.default_provider != DefaultProvider::None) {
        usage += compat::format(" [{}=@]", arg.name);
      }
      else {
        usage += compat::format(" [{}]", arg.name);
      }
    }
    else {
      // Required args
      if (arg.allowed_values && !arg.allowed_values->empty()) {
        // Show allowed values inline for small sets
        std::string values;
        for (size_t i = 0; i < arg.allowed_values->size(); ++i) {
          if (i > 0) {
            values += "/";
          }
          values += (*arg.allowed_values)[i];
        }
        usage += compat::format(" <{}>", values);
      }
      else {
        usage += compat::format(" <{}>", arg.name);
      }
    }
  }

  return usage;
}

CommandResult CommandParser::too_many_arguments_error(const CommandDef& cmd,
                                                      size_t expected,
                                                      size_t got) {
  std::string error =
      compat::format("Too many arguments: got {}, expected {}", got, expected);
  std::string suggestion = compat::format("Try 'help {}' for usage", cmd.name);
  return CommandResult::invalid_syntax(error, suggestion);
}

} // namespace debug::command
