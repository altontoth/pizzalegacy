/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/command/CommandRegistry.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "CommandContext.h"
#include "core/debug/command/CommandTokenizer.h"
#include "core/state/GameState.h"
#include "core/util/GameContext.h"
#include "engine/Logger.h"

namespace debug::command {

// Argument to 'help' that dumps every command and subcommand.
constexpr std::string_view all_keyword = "all";

static void sort_by_name(std::vector<const CommandDef*>& cmds) {
  std::ranges::sort(cmds, {}, &CommandDef::name);
}

// "restaurant (r, rest)", or just the name when there are no aliases.
static std::string label_with_aliases(const CommandDef& cmd) {
  if (cmd.aliases.empty()) {
    return cmd.name;
  }

  std::string joined;
  for (const auto& alias : cmd.aliases) {
    if (!joined.empty()) {
      joined += ", ";
    }
    joined += alias;
  }
  return compat::format("{} ({})", cmd.name, joined);
}

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

  if (command_path.empty() || command_path == all_keyword) {
    const bool full_tree = (command_path == all_keyword);

    std::vector<const CommandDef*> cmds;
    cmds.reserve(commands_.size());
    for (const auto& [name, cmd] : commands_) {
      cmds.push_back(&cmd);
    }
    sort_by_name(cmds);

    std::vector<std::string> labels;
    labels.reserve(cmds.size());
    std::size_t width = 0;
    for (const auto* cmd : cmds) {
      labels.push_back(label_with_aliases(*cmd));
      width = std::max(width, labels.back().size());
    }

    result.push_back("Available commands:");
    for (std::size_t i = 0; i < cmds.size(); ++i) {
      result.push_back(compat::format("  {:<{}}  {}", labels[i], width,
                                      cmds[i]->description));
      if (full_tree) {
        collect_subcommand_help(*cmds[i], "  " + cmds[i]->name, result);
      }
    }

    if (!full_tree) {
      result.push_back(
          compat::format("Use 'help <command>', or 'help {}' for every "
                         "subcommand",
                         all_keyword));
    }
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

    if (!cmd.subcommands.empty()) {
      result.push_back("Subcommands:");
      collect_subcommand_help(cmd, cmd.name, result);
    }
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
    CommandContext ctx{state, context, *this, std::move(parsed.args)};
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
  std::vector<const CommandDef*> subs;
  subs.reserve(cmd.subcommands.size());
  for (const auto& [name, subcmd] : cmd.subcommands) {
    subs.push_back(&subcmd);
  }
  sort_by_name(subs);

  for (const auto* subcmd : subs) {
    std::string full_name = prefix.empty()
                                ? subcmd->name
                                : compat::format("{} {}", prefix, subcmd->name);
    output.push_back(compat::format("{} - {}", full_name, subcmd->description));

    // Recurse for nested subcommands
    if (!subcmd->subcommands.empty()) {
      collect_subcommand_help(*subcmd, full_name, output);
    }
  }
}
} // namespace debug::command
