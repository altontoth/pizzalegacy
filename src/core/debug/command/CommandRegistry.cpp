/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/command/CommandRegistry.h"
#include "CommandContext.h"
#include "core/debug/command/CommandTokenizer.h"
#include "core/state/GameState.h"
#include "core/util/GameContext.h"
#include "engine/Logger.h"

namespace debug::command {

void CommandRegistry::register_command(CommandDef cmd) {
  std::string name = cmd.name;

  // Register all aliases
  for (const auto& alias : cmd.aliases) {
    aliases_[alias] = name;
  }

  // Store command by primary name
  commands_[name] = std::move(cmd);
}

std::optional<std::reference_wrapper<const CommandDef>>
CommandRegistry::find_command(const std::string& name) const {
  // Try direct lookup
  auto it = commands_.find(name);
  if (it != commands_.end()) {
    return std::cref(it->second);
  }

  // Try alias lookup
  auto alias_it = aliases_.find(name);
  if (alias_it != aliases_.end()) {
    auto cmd_it = commands_.find(alias_it->second);
    if (cmd_it != commands_.end()) {
      return std::cref(cmd_it->second);
    }
  }

  return std::nullopt;
}

std::vector<const CommandDef*>
CommandRegistry::get_commands_by_category(const std::string& category) const {
  std::vector<const CommandDef*> result;

  for (const auto& [name, cmd] : commands_) {
    if (cmd.category == category) {
      result.push_back(&cmd);
    }
  }

  return result;
}

std::vector<std::string> CommandRegistry::get_all_command_names() const {
  std::vector<std::string> result;

  for (const auto& [name, cmd] : commands_) {
    result.push_back(name);
  }

  return result;
}

std::vector<std::string>
CommandRegistry::get_help(const std::string& command_path) const {
  std::vector<std::string> result;

  if (command_path.empty()) {
    // General help - list all commands by category
    result.push_back("Available commands:");
    result.push_back("");

    // Group by category
    std::unordered_map<std::string, std::vector<const CommandDef*>> by_category;
    for (const auto& [name, cmd] : commands_) {
      std::string category = cmd.category.empty() ? "general" : cmd.category;
      by_category[category].push_back(&cmd);
    }

    // Output each category
    for (const auto& [category, cmds] : by_category) {
      result.push_back(compat::format("{}:", category));
      for (const auto* cmd : cmds) {
        result.push_back(
            compat::format("  {} - {}", cmd->name, cmd->description));
      }
      result.push_back("");
    }

    result.push_back("Use 'help <command>' for more information");
  }
  else {
    // Specific command help
    auto cmd_ref = find_command(command_path);
    if (!cmd_ref) {
      result.push_back(compat::format("Unknown command: {}", command_path));
      return result;
    }

    const auto& cmd = cmd_ref->get();
    auto cmd_help = format_command_help(cmd);
    result.insert(result.end(), cmd_help.begin(), cmd_help.end());
  }

  return result;
}

CommandResult CommandRegistry::execute(const std::string& line,
                                       GameState& state, GameContext& context) {
  // Tokenize the input
  auto tokens = CommandTokenizer::tokenize_simple(line);
  if (tokens.empty()) {
    return CommandResult::invalid_syntax("No command entered");
  }

  // Find the root command
  auto cmd_ref = find_command(tokens[0]);
  if (!cmd_ref) {
    auto result = CommandResult::invalid_syntax(
        compat::format("Unknown command: '{}'", tokens[0]),
        "Try 'help' for a list of commands");
    Logger::warn("Command error: {}", *result.error_message);
    return result;
  }

  const CommandDef& root = cmd_ref->get();

  // Parse the command and arguments
  auto parsed = parser_.parse(root, tokens, state);
  if (!parsed.success) {
    // Log the parsing error so user knows why it failed
    if (parsed.error_result.error_message) {
      Logger::warn("Command parsing failed: {}",
                   *parsed.error_result.error_message);
      if (parsed.error_result.error_suggestion) {
        Logger::warn("  Suggestion: {}", *parsed.error_result.error_suggestion);
      }
    }
    return parsed.error_result;
  }

  // Execute the command handler
  try {
    CommandContext ctx{state, context, std::move(parsed.args)};
    return parsed.command->handler(ctx);
  }
  catch (const std::exception& e) {
    auto result = CommandResult::error(
        compat::format("Command execution failed: {}", e.what()));
    Logger::warn("Command execution error: {}", e.what());
    return result;
  }
}

std::vector<std::string>
CommandRegistry::format_command_help(const CommandDef& cmd) const {
  std::vector<std::string> result;

  // Command name and description
  result.push_back(compat::format("Command: {}", cmd.name));
  if (!cmd.aliases.empty()) {
    std::string aliases_str;
    for (size_t i = 0; i < cmd.aliases.size(); ++i) {
      if (i > 0)
        aliases_str += ", ";
      aliases_str += cmd.aliases[i];
    }
    result.push_back(compat::format("Aliases: {}", aliases_str));
  }
  result.push_back(compat::format("Description: {}", cmd.description));
  result.push_back("");

  // Arguments
  if (!cmd.args.empty()) {
    result.push_back("Arguments:");
    for (const auto& arg : cmd.args) {
      std::string arg_desc = compat::format(
          "  {} ({}) - {}", arg.name, type_name(arg.type), arg.description);
      if (arg.optional) {
        arg_desc += " [optional]";
      }
      result.push_back(arg_desc);
    }
    result.push_back("");
  }

  // Subcommands
  if (!cmd.subcommands.empty()) {
    result.push_back("Subcommands:");
    for (const auto& [name, subcmd] : cmd.subcommands) {
      result.push_back(compat::format("  {} - {}", name, subcmd.description));
    }
    result.push_back("");
  }

  // Detailed help text
  if (!cmd.help_text.empty()) {
    result.push_back(cmd.help_text);
  }

  return result;
}

void CommandRegistry::collect_subcommand_help(
    const CommandDef& cmd, const std::string& prefix,
    std::vector<std::string>& output) const {
  for (const auto& [name, subcmd] : cmd.subcommands) {
    std::string full_name =
        prefix.empty() ? name : compat::format("{} {}", prefix, name);
    output.push_back(compat::format("{} - {}", full_name, subcmd.description));

    // Recurse for nested subcommands
    if (!subcmd.subcommands.empty()) {
      collect_subcommand_help(subcmd, full_name, output);
    }
  }
}
} // namespace debug::command
