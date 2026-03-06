/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>

#include "CommandDef.h"
#include "CommandResult.h"
#include "ParsedValue.h"

// Forward declaration
class GameState;

namespace debug::command {

// Command parser - walks command tree and parses arguments
class CommandParser {
public:
  // Result of parsing a command line
  struct ParsedCommand {
    const CommandDef* command; // Final command node (or nullptr if not found)
    std::vector<ParsedValue> args; // Parsed and validated arguments
    CommandResult error_result;    // If parsing failed
    bool success;
  };

  // Main parsing entry point
  ParsedCommand parse(const CommandDef& root,
                      const std::vector<std::string>& tokens, GameState& state);

private:
  // Result of finding a command in the tree
  struct CommandMatch {
    const CommandDef* command;
    size_t tokens_consumed; // How many tokens were command/subcommand names
  };

  // Walk command tree to find deepest matching command
  CommandMatch find_command(const CommandDef& root,
                            const std::vector<std::string>& tokens);

  // Parse a single argument with provenance tracking
  std::optional<ParsedValue> parse_argument(const std::string& token,
                                            const ArgSpec& spec,
                                            GameState& state,
                                            ParsedCommand& result);

  // Check if token is '@' default marker
  bool is_default_marker(const std::string& token) const;

  // Generate error for missing required argument
  CommandResult missing_argument_error(const CommandDef& cmd,
                                       const ArgSpec& spec, size_t provided);

  // Build usage string showing all arguments
  std::string build_usage_string(const CommandDef& cmd) const;

  // Generate error for too many arguments
  CommandResult too_many_arguments_error(const CommandDef& cmd, size_t expected,
                                         size_t got);
};

} // namespace debug::command
