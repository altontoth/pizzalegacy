/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "GameCommands.h"
#include "core/save/SaveGameManager.h"
#include "core/state/GameState.h"

using namespace debug::command;

namespace debug::commands {

static CommandDef make_game_save() {
  return CommandDef{
      .name = "save",
      .description = "Save the game to a file",
      .help_text = R"(Usage: game save <filename>

Saves the current game state to a JSON file.
The .json extension is added automatically if not provided.

Examples:
  game save mygame         # Saves to mygame.json
  game save backup.json    # Saves to backup.json
  game save test           # Saves to test.json
)",
      .args =
          {
              ArgSpec{
                  .name = "filename",
                  .type = ArgType::String,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Filename to save to (.json added if needed)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        std::string filename = ctx.string_arg(1);

        // Add .json extension if not present
        if (filename.size() < 5 ||
            filename.substr(filename.size() - 5) != ".json") {
          filename += ".json";
        }

        bool success =
            save::SaveGameManager::save(ctx.state, ctx.context, filename);

        if (success) {
          return CommandResult::success().add_message(
              compat::format("Game saved to: {}", filename));
        }
        else {
          return CommandResult::error(
              compat::format("Failed to save game to: {}", filename));
        }
      },
      .category = "game"};
}

static CommandDef make_game_load() {
  return CommandDef{
      .name = "load",
      .description = "Load a game from a file",
      .help_text = R"(Usage: game load <filename>

Loads a saved game from a JSON file.
The .json extension is added automatically if not provided.

Examples:
  game load mygame         # Loads from mygame.json
  game load backup.json    # Loads from backup.json
  game load test           # Loads from test.json
)",
      .args =
          {
              ArgSpec{
                  .name = "filename",
                  .type = ArgType::String,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description =
                      "Filename to load from (.json added if needed)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        std::string filename = ctx.string_arg(1);

        // Add .json extension if not present
        if (filename.size() < 5 ||
            filename.substr(filename.size() - 5) != ".json") {
          filename += ".json";
        }

        // Check if file is valid first
        if (!save::SaveGameManager::is_valid_save(filename)) {
          return CommandResult::error(
              compat::format("Invalid save file: {}", filename),
              "File does not exist or is not a valid save");
        }

        int version = save::SaveGameManager::get_save_version(filename);
        CommandResult result = CommandResult::success();
        result.add_message(
            compat::format("Loading save file (version {})...", version));

        bool success =
            save::SaveGameManager::load(ctx.state, ctx.context, filename);

        if (success) {
          result.add_message(compat::format("Game loaded from: {}", filename));
        }
        else {
          return CommandResult::error(
              compat::format("Failed to load game from: {}", filename));
        }

        return result;
      },
      .category = "game"};
}

static CommandDef make_game_exit() {
  return CommandDef{.name = "exit",
                    .aliases = {"quit"},
                    .description = "Exit the game",
                    .help_text = R"(Usage: game exit

Exits the game immediately without saving.

Example:
  game exit
  game quit        # Alternative command
)",
                    .handler = [](const CommandContext& ctx) -> CommandResult {
                      ctx.state.request_exit();
                      return CommandResult::success().add_message(
                          "Exiting game...");
                    },
                    .category = "game"};
}

CommandDef make_game_commands() {
  return CommandDef{
      .name = "game",
      .description = "Game state management",
      .help_text = "Manage game state: save, load, exit",
      .category = "game",
  }
      .add_subcommand(make_game_save())
      .add_subcommand(make_game_load())
      .add_subcommand(make_game_exit());
}

} // namespace debug::commands
