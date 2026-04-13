/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/commands/DebugCommands.h"

#include <sstream>
#include "core/state/GameState.h"
#include "core/util/GameContext.h"
#include "engine/input/InputManager.h"
#include "engine/ui/InteractionManager.h"

using namespace debug::command;

namespace debug::commands {

static CommandDef make_debug_dump_player() {
  return CommandDef{
      .name = "player",
      .description = "Dump player information",
      .help_text = R"(Usage: debug dump player [player_id]

Dumps detailed information about a player.

Examples:
  debug dump player @      # Active player
  debug dump player 0      # Player 0
)",
      .args =
          {
              ArgSpec{
                  .name = "player_id",
                  .type = ArgType::PlayerId,
                  .optional = true,
                  .default_provider = DefaultProvider::ActivePlayer,
                  .description = "Player ID (@ for active player)",
                  .min_value = 0,
                  .max_value = 7,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto* player = ctx.state.player(player_id);

        if (!player) {
          return CommandResult::error(
              compat::format("Player {} does not exist", player_id.value));
        }

        std::ostringstream ss;
        ss << *player;

        std::string output = ss.str();
        std::istringstream stream(output);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(stream, line)) {
          lines.push_back(line);
        }

        CommandResult result = CommandResult::success();
        for (const auto& l : lines) {
          result.add_message(l);
        }
        return result;
      },
      .category = "debug"};
}

static CommandDef make_debug_dump_players() {
  return CommandDef{.name = "players",
                    .description = "Dump all players",
                    .help_text = R"(Usage: debug dump players

Dumps information about all players in the game.

Example:
  debug dump players
)",
                    .handler = [](const CommandContext& ctx) -> CommandResult {
                      std::ostringstream ss;
                      for (auto player : ctx.state.players()) {
                        ss << *player << "\n";
                      }

                      std::string output = ss.str();
                      std::istringstream stream(output);
                      std::vector<std::string> lines;
                      std::string line;
                      while (std::getline(stream, line)) {
                        lines.push_back(line);
                      }

                      CommandResult result = CommandResult::success();
                      for (const auto& l : lines) {
                        result.add_message(l);
                      }
                      return result;
                    },
                    .category = "debug"};
}

static CommandDef make_debug_dump_ledger() {
  return CommandDef{
      .name = "ledger",
      .description = "Dump player financials",
      .help_text = R"(Usage: debug dump ledger [player_id]

Dumps the financial ledger for a player.

Examples:
  debug dump ledger @      # Active player
  debug dump ledger 0      # Player 0
)",
      .args =
          {
              ArgSpec{
                  .name = "player_id",
                  .type = ArgType::PlayerId,
                  .optional = true,
                  .default_provider = DefaultProvider::ActivePlayer,
                  .description = "Player ID (@ for active player)",
                  .min_value = 0,
                  .max_value = 7,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto* player = ctx.state.player(player_id);

        if (!player) {
          return CommandResult::error(
              compat::format("Player {} does not exist", player_id.value));
        }

        std::ostringstream ss;
        ss << player->financials();

        std::string output = ss.str();
        std::istringstream stream(output);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(stream, line)) {
          lines.push_back(line);
        }

        CommandResult result = CommandResult::success();
        for (const auto& l : lines) {
          result.add_message(l);
        }
        return result;
      },
      .category = "debug"};
}

static CommandDef make_debug_dump_state() {
  return CommandDef{.name = "state",
                    .description = "Dump entire game state",
                    .help_text = R"(Usage: debug dump state

Dumps the entire game state.

Example:
  debug dump state
)",
                    .handler = [](const CommandContext& ctx) -> CommandResult {
                      std::ostringstream ss;
                      ss << ctx.state;

                      std::string output = ss.str();
                      std::istringstream stream(output);
                      std::vector<std::string> lines;
                      std::string line;
                      while (std::getline(stream, line)) {
                        lines.push_back(line);
                      }

                      CommandResult result = CommandResult::success();
                      for (const auto& l : lines) {
                        result.add_message(l);
                      }
                      return result;
                    },
                    .category = "debug"};
}

static CommandDef make_debug_dump_layers() {
  return CommandDef{.name = "layers",
                    .description = "Dump interaction layers",
                    .help_text = R"(Usage: debug dump layers

Dumps the current interaction manager layer stack.

Example:
  debug dump layers
)",
                    .handler = [](const CommandContext& ctx) -> CommandResult {
                      std::ostringstream ss;
                      ctx.context.interaction->dump_layers(ss);

                      std::string output = ss.str();
                      std::istringstream stream(output);
                      std::vector<std::string> lines;
                      std::string line;
                      while (std::getline(stream, line)) {
                        lines.push_back(line);
                      }

                      CommandResult result = CommandResult::success();
                      for (const auto& l : lines) {
                        result.add_message(l);
                      }
                      return result;
                    },
                    .category = "debug"};
}

static CommandDef make_debug_dump_inventory() {
  return CommandDef{
      .name = "inventory",
      .description = "Dump player inventory",
      .help_text = R"(Usage: debug dump inventory [player_id]

Dumps the inventory for a player.

Examples:
  debug dump inventory @   # Active player
  debug dump inventory 0   # Player 0
)",
      .args =
          {
              ArgSpec{
                  .name = "player_id",
                  .type = ArgType::PlayerId,
                  .optional = true,
                  .default_provider = DefaultProvider::ActivePlayer,
                  .description = "Player ID (@ for active player)",
                  .min_value = 0,
                  .max_value = 7,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto* player = ctx.state.player(player_id);

        if (!player) {
          return CommandResult::error(
              compat::format("Player {} does not exist", player_id.value));
        }

        std::ostringstream ss;
        ss << player->inventory();

        std::string output = ss.str();
        std::istringstream stream(output);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(stream, line)) {
          lines.push_back(line);
        }

        CommandResult result = CommandResult::success();
        for (const auto& l : lines) {
          result.add_message(l);
        }
        return result;
      },
      .category = "debug"};
}

static CommandDef make_debug_dump_input() {
  return CommandDef{
      .name = "input",
      .description = "Dump input system state",
      .help_text = R"(Usage: debug dump input <bindings|state>

Dumps input system information.
- bindings: Show key bindings
- state: Show current input state

Examples:
  debug dump input bindings
  debug dump input state
)",
      .args =
          {
              ArgSpec{
                  .name = "type",
                  .type = ArgType::String,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "What to dump (bindings or state)",
                  .allowed_values =
                      std::vector<std::string>{"bindings", "state"},
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto type = ctx.string_arg(0);

        std::ostringstream ss;
        if (type == "bindings") {
          ctx.context.input->debug_dump_bindings(ss);
        }
        else if (type == "state") {
          ctx.context.input->debug_dump_state(ss);
        }
        else {
          return CommandResult::error(
              "Invalid type, must be 'bindings' or 'state'");
        }

        std::string output = ss.str();
        std::istringstream stream(output);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(stream, line)) {
          lines.push_back(line);
        }

        CommandResult result = CommandResult::success();
        for (const auto& l : lines) {
          result.add_message(l);
        }
        return result;
      },
      .category = "debug"};
}

static CommandDef make_debug_dump() {
  return CommandDef{
      .name = "dump",
      .description = "Dump debugging information",
      .help_text = "Dump various game state for debugging",
      .category = "debug",
  }
      .add_subcommand(make_debug_dump_player())
      .add_subcommand(make_debug_dump_players())
      .add_subcommand(make_debug_dump_ledger())
      .add_subcommand(make_debug_dump_state())
      .add_subcommand(make_debug_dump_layers())
      .add_subcommand(make_debug_dump_inventory())
      .add_subcommand(make_debug_dump_input());
}

static CommandDef make_debug_test() {
  return CommandDef{.name = "test",
                    .description = "Test status message system",
                    .help_text = R"(Usage: debug test

Posts a test message to the status hub.

Example:
  debug test
)",
                    .handler = [](const CommandContext& ctx) -> CommandResult {
                      StatusMsg msg;
                      msg.payload = "test 123";
                      msg.ttl = std::chrono::milliseconds{2500};
                      ctx.state.status().post_direct(msg);
                      return CommandResult::success().add_message(
                          "Test message posted to status hub");
                    },
                    .category = "debug"};
}

CommandDef make_debug_commands() {
  return CommandDef{
      .name = "debug",
      .description = "Debug and diagnostic commands",
      .help_text = "Debug utilities for inspecting game state",
      .category = "debug",
  }
      .add_subcommand(make_debug_dump())
      .add_subcommand(make_debug_test());
}

CommandDef make_dump_command() {
  return CommandDef{
      .name = "dump",
      .description = "Dump debugging information (alias for 'debug dump')",
      .help_text = "Dump various game state for debugging",
      .category = "general",
  }
      .add_subcommand(make_debug_dump_player())
      .add_subcommand(make_debug_dump_players())
      .add_subcommand(make_debug_dump_ledger())
      .add_subcommand(make_debug_dump_state())
      .add_subcommand(make_debug_dump_layers())
      .add_subcommand(make_debug_dump_inventory())
      .add_subcommand(make_debug_dump_input());
}

CommandDef make_test_command() {
  return CommandDef{.name = "test",
                    .description = "Test status message system",
                    .help_text = R"(Usage: test

Posts a test message to the status hub.

Example:
  test
)",
                    .handler = [](const CommandContext& ctx) -> CommandResult {
                      StatusMsg msg;
                      msg.payload = "test 123";
                      msg.ttl = std::chrono::milliseconds{2500};
                      ctx.state.status().post_direct(msg);
                      return CommandResult::success().add_message(
                          "Test message posted to status hub");
                    },
                    .category = "general"};
}

CommandDef make_help_command() {
  return CommandDef{
      .name = "help",
      .description = "Show help for commands",
      .help_text = R"(Usage: help

Shows help information for available commands.

Example:
  help
)",
      .handler = [](const CommandContext& /*ctx*/) -> CommandResult {
        CommandResult result = CommandResult::success();

        result.add_message("Available commands:");
        result.add_message("==================");
        result.add_message("");
        result.add_message("  player     - Player management commands");
        result.add_message("  restaurant - Restaurant management commands");
        result.add_message("  depot      - Depot management commands");
        result.add_message(
            "  game       - Game state commands (save/load/exit)");
        result.add_message("  nav        - Navigation commands");
        result.add_message("  debug      - Debug and diagnostic commands");
        result.add_message("  city       - City commands (pizza charts)");
        result.add_message("  news       - Newspaper and trend system");
        result.add_message("  dump       - Dump debugging information");
        result.add_message("  test       - Test status message system");
        result.add_message("");
        result.add_message("Type a command name to see its subcommands.");
        result.add_message("Example: player, dump, restaurant");

        return result;
      },
      .category = "general"};
}

} // namespace debug::commands
