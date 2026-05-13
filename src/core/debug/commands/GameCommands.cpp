/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "GameCommands.h"
#include "core/events/LeventId.h"
#include "core/save/SaveGameManager.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
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

static CommandDef make_game_competition_monthly() {
  return CommandDef{.name = "monthly",
                    .description = "Trigger the monthly pizza competition",
                    .help_text = R"(Usage: game competition monthly

Immediately triggers the monthly pizza baking competition for the active
player, bypassing the normal end-of-month schedule. The player must have
at least $500 cash. The normal participation, betting, and speed dialogs
will appear.

Example:
  game competition monthly
)",
                    .handler = [](const CommandContext& ctx) -> CommandResult {
                      ctx.state.run_monthly_competition();
                      return CommandResult::success().add_message(
                          "Monthly pizza competition triggered");
                    },
                    .category = "game"};
}

static CommandDef make_game_end() {
  return CommandDef{
      .name = "end",
      .description = "Trigger game over for the active player",
      .help_text = R"(Usage: game end <cause>

Immediately triggers game over for the active player.
Valid causes: debts, suppliers, killed, tycoon
)",
      .args =
          {
              ArgSpec{
                  .name = "cause",
                  .type = ArgType::String,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Game over cause",
                  .allowed_values =
                      std::vector<std::string>{"debts", "killed", "suppliers",
                                               "tycoon"},
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* player = ctx.state.active_player();
        if (!player) {
          return CommandResult::error("No active player");
        }

        if (ctx.args.empty()) {
          return CommandResult::error(
              "Missing cause argument",
              "Usage: game end <debts|killed|tycoon|suppliers>");
        }

        std::string cause_str = ctx.string_arg(0);

        GameOverCause cause;
        core::events::LeventId message;
        if (cause_str == "killed") {
          cause = GameOverCause::KilledByMafia;
          message =
              core::events::LeventId::CoolVinnieAndBigLuigiTakeYouForABath;
        }
        else if (cause_str == "tycoon") {
          ctx.state.trigger_win_sequence(player->id());
          return CommandResult::success().add_message(
              "Game over triggered: tycoon");
        }
        else if (cause_str == "suppliers") {
          cause = GameOverCause::BankruptSuppliers;
          message = core::events::LeventId::NotAbleToPaySuppliers;
        }
        else {
          cause = GameOverCause::BankruptDebts;
          message = core::events::LeventId::NotAbleToPayDebts;
        }

        ctx.state.deliver_message(message,
                                  [player, cause, &state = ctx.state]() {
                                    state.eliminate_player(player->id(), cause);
                                  });
        return CommandResult::success().add_message(
            compat::format("Game over triggered: {}", cause_str));
      },
      .category = "game"};
}

static CommandDef make_game_competition() {
  return CommandDef{
      .name = "competition",
      .description = "Pizza competition commands",
      .help_text = "Trigger pizza competitions for testing",
      .category = "game",
  }
      .add_subcommand(make_game_competition_monthly());
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
      .add_subcommand(make_game_date())
      .add_subcommand(make_game_competition())
      .add_subcommand(make_game_end());
}

} // namespace debug::commands
