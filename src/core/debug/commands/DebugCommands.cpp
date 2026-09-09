/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/commands/DebugCommands.h"

#include <fstream>
#include <sstream>
#include "core/debug/command/CommandRegistry.h"
#include "core/debug/command/CommonArgs.h"
#include "core/save/SaveGameManager.h"
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
              active_player_arg(),
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
  };
}

static CommandDef make_debug_dump_players() {
  return CommandDef{
      .name = "players",
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
  };
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
              active_player_arg(),
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
  };
}

static CommandDef make_debug_dump_state() {
  return CommandDef{
      .name = "state",
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
  };
}

static CommandDef make_debug_dump_fingerprint() {
  return CommandDef{
      .name = "fingerprint",
      .description = "Print a hash of the entire game state",
      .help_text = R"(Usage: dump fingerprint [file]

Hashes the save document, minus the timestamp, so two runs of the same script
can be compared. Give a file to write the document that was hashed; when a
fingerprint does not match, diffing those two files says which field moved.

Only meaningful when nothing is in progress: mid-turn the pipeline holds state
that never reaches a save.

Examples:
  dump fingerprint
  dump fingerprint state.json
)",
      .args =
          {
              ArgSpec{
                  .name = "file",
                  .type = ArgType::String,
                  .optional = true,
                  .description = "Where to write the hashed document",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        const save::StateFingerprint fp =
            save::SaveGameManager::fingerprint(ctx.state, ctx.context);
        const std::string document = fp.document.dump(2);

        CommandResult result = CommandResult::success();
        result.add_message(fp.hash);

        if (ctx.has_arg(0)) {
          const std::string file = ctx.string_arg(0);
          std::ofstream out(file);
          if (!out) {
            return CommandResult::error(
                compat::format("Cannot write {}", file));
          }
          out << document << "\n";
          if (!out) {
            return CommandResult::error(
                compat::format("Failed writing {}", file));
          }
          result.add_message(compat::format("Wrote {}", file));
        }

        return result.with_json(document);
      },
  };
}

static CommandDef make_debug_dump_layers() {
  return CommandDef{
      .name = "layers",
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
  };
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
              active_player_arg(),
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
  };
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
  };
}

static CommandDef make_debug_dump() {
  return CommandDef{
      .name = "dump",
      .description = "Dump debugging information",
      .help_text = "Dump various game state for debugging",
  }
      .add_subcommand(make_debug_dump_player())
      .add_subcommand(make_debug_dump_players())
      .add_subcommand(make_debug_dump_ledger())
      .add_subcommand(make_debug_dump_state())
      .add_subcommand(make_debug_dump_fingerprint())
      .add_subcommand(make_debug_dump_layers())
      .add_subcommand(make_debug_dump_inventory())
      .add_subcommand(make_debug_dump_input());
}

CommandDef make_debug_commands() {
  return CommandDef{
      .name = "debug",
      .description = "Debug and diagnostic commands",
      .help_text = "Debug utilities for inspecting game state",
  }
      .add_subcommand(make_debug_dump());
}

CommandDef make_dump_command() {
  return CommandDef{
      .name = "dump",
      .description = "Dump debugging information (alias for 'debug dump')",
      .help_text = "Dump various game state for debugging",
  }
      .add_subcommand(make_debug_dump_player())
      .add_subcommand(make_debug_dump_players())
      .add_subcommand(make_debug_dump_ledger())
      .add_subcommand(make_debug_dump_state())
      .add_subcommand(make_debug_dump_fingerprint())
      .add_subcommand(make_debug_dump_layers())
      .add_subcommand(make_debug_dump_inventory())
      .add_subcommand(make_debug_dump_input());
}

CommandDef make_help_command() {
  return CommandDef{
      .name = "help",
      .description = "Show help for commands",
      .help_text = R"(Usage: help [command]

Lists every registered command. Name a command for its
arguments and subcommands, or "all" for the whole tree at once.

Examples:
  help
  help player
  help all
)",
      .args =
          {
              ArgSpec{
                  .name = "command",
                  .type = ArgType::String,
                  .optional = true,
                  .description = R"(Command to describe, or "all")",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        const std::string path = ctx.has_arg(0) ? ctx.string_arg(0) : "";

        CommandResult result = CommandResult::success();
        for (const auto& line : ctx.registry.get_help(path)) {
          result.add_message(line);
        }
        return result;
      },
  };
}

} // namespace debug::commands
