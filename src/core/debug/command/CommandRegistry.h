/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "CommandDef.h"
#include "CommandParser.h"
#include "CommandResult.h"

// Forward declarations
class GameState;
class GameContext;

namespace debug::command {

// Command registry - central repository for all commands
class CommandRegistry {
public:
  // Register a command (stores by name and all aliases)
  void register_command(CommandDef cmd);

  // Find a command by name or alias
  std::optional<std::reference_wrapper<const CommandDef>>
  find_command(const std::string& name) const;

  // Get all command names (for completion)
  std::vector<std::string> get_all_command_names() const;

  // Get all aliases and their target command names
  const std::unordered_map<std::string, std::string>& get_aliases() const {
    return aliases_;
  }

  // Generate help text
  std::vector<std::string> get_help(const std::string& command_path = "") const;

  // Execute a command from a string; the line is split into tokens, honouring
  // quotes and backslash escapes
  CommandResult execute(const std::string& line, GameState& state,
                        GameContext& context);

  // Execute a command from already separated arguments, e.g. argv
  CommandResult execute(const std::vector<std::string>& tokens,
                        GameState& state, GameContext& context);

private:
  std::unordered_map<std::string, CommandDef> commands_;
  std::unordered_map<std::string, std::string> aliases_; // alias -> real name

  CommandParser parser_;

  // Helper to format help for a command
  std::vector<std::string> format_command_help(const CommandDef& cmd) const;

  // Helper to collect subcommand help recursively
  void collect_subcommand_help(const CommandDef& cmd, const std::string& prefix,
                               std::vector<std::string>& output) const;
};

} // namespace debug::command
