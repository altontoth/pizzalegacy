/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "GameCommands.h"
#include "core/save/SaveGameManager.h"
#include "core/state/GameState.h"
#include "core/time/GameDate.h"
#include "core/time/GameTime.h"

using namespace debug::command;

namespace debug::commands {

static CommandDef make_game_save() {
  return CommandDef{
      .name = "save",
      .description = "Save the game to a file",
      .help_text = R"(Usage: game save <filename>

Saves the current game state to a compressed .plsave file.
The .plsave extension is added automatically if not provided.

Examples:
  game save mygame           # Saves to mygame.plsave
  game save backup.plsave    # Saves to backup.plsave
  game save test             # Saves to test.plsave
)",
      .args =
          {
              ArgSpec{
                  .name = "filename",
                  .type = ArgType::String,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description =
                      "Filename to save to (.plsave added if needed)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        std::string filename = ctx.string_arg(0);

        // Add .plsave extension if not present
        if (!filename.ends_with(".plsave")) {
          filename += ".plsave";
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

Loads a saved game from a compressed .plsave file.
The .plsave extension is added automatically if not provided.

Examples:
  game load mygame           # Loads from mygame.plsave
  game load backup.plsave    # Loads from backup.plsave
  game load test             # Loads from test.plsave
)",
      .args =
          {
              ArgSpec{
                  .name = "filename",
                  .type = ArgType::String,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description =
                      "Filename to load from (.plsave added if needed)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        std::string filename = ctx.string_arg(0);

        // Add .plsave extension if not present
        if (!filename.ends_with(".plsave")) {
          filename += ".plsave";
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

static CommandDef make_game_date_set() {
  return CommandDef{
      .name = "set",
      .description = "Set the game date",
      .help_text = R"(Usage: game date set <dd-mm-yyyy>

Sets the game date, preserving the current time of day.

Examples:
  game date set 15-01-1996    # Winter
  game date set 01-06-1995    # Summer
  game date set 10-10-1995    # Autumn
)",
      .args =
          {
              ArgSpec{
                  .name = "date",
                  .type = ArgType::String,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Date in dd-mm-yyyy format",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        std::string date_str = ctx.string_arg(0);

        int day = 0;
        int month = 0;
        int year = 0;
        if (sscanf(date_str.c_str(), "%d-%d-%d", &day, &month, &year) != 3) {
          return CommandResult::error("Invalid date format, use dd-mm-yyyy");
        }
        if (month < 1 || month > 12 || day < 1 || day > 31 || year < 1995) {
          return CommandResult::error("Invalid date values");
        }

        GameDate target(year, month, day);
        int days = target.days_since(game_start_date);
        if (days < 0) {
          return CommandResult::error("Date must be on or after 01-03-1995");
        }

        constexpr int BLOCKS_PER_DAY = 144;
        auto time = ctx.state.time();
        int time_of_day = time.time_blocks() % BLOCKS_PER_DAY;
        time.time_blocks(days * BLOCKS_PER_DAY + time_of_day);
        ctx.state.set_time(time);

        auto season = GameTime::to_string(time.season());
        return CommandResult::success().add_message(compat::format(
            "Date set to {} ({})", time.to_date_string(), season));
      },
      .category = "game"};
}

static CommandDef make_game_date() {
  return CommandDef{
      .name = "date",
      .description = "Date management",
      .help_text = "View or change the game date",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto& time = ctx.state.time();
        auto season = GameTime::to_string(time.season());
        return CommandResult::success().add_message(
            compat::format("{} ({})", time.to_date_string(), season));
      },
      .category = "game",
  }
      .add_subcommand(make_game_date_set());
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
      .add_subcommand(make_game_exit())
      .add_subcommand(make_game_date());
}

} // namespace debug::commands
