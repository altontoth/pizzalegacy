/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "GameCommands.h"
#include "core/events/CareerLog.h"
#include "core/events/MilestoneId.h"
#include "core/mission/MissionFile.h"
#include "core/mission/MissionScoring.h"
#include "core/mission/MissionTarget.h"
#include "core/save/SaveGameManager.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/time/GameDate.h"
#include "core/time/GameTime.h"

#include <cstdio>

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
  };
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
  };
}

static CommandDef make_game_exit() {
  return CommandDef{
      .name = "exit",
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
        return CommandResult::success().add_message("Exiting game...");
      },
  };
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
  };
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
  }
      .add_subcommand(make_game_date_set());
}

static CommandDef make_game_time_set() {
  return CommandDef{
      .name = "set",
      .description = "Set the time of day",
      .help_text = R"(Usage: game time set <hh:mm>

Sets the time of day, preserving the current date. Time advances in
10-minute blocks, so minutes are rounded down to the nearest 10.

Example:
  game time set 09:00
)",
      .args =
          {
              ArgSpec{
                  .name = "time",
                  .type = ArgType::String,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Time in hh:mm format",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        std::string time_str = ctx.string_arg(0);

        int hour = 0;
        int minute = 0;
        if (sscanf(time_str.c_str(), "%d:%d", &hour, &minute) != 2) {
          return CommandResult::error("Invalid time format, use hh:mm");
        }
        if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
          return CommandResult::error("Invalid time values");
        }

        auto time = ctx.state.time();
        time.set_time_of_day(hour, minute);
        ctx.state.set_time(time);

        return CommandResult::success().add_message(
            compat::format("Time set to {:02d}:{:02d}", time.current_hour(),
                           time.current_minute()));
      },
  };
}

static CommandDef make_game_time() {
  return CommandDef{
      .name = "time",
      .description = "Time of day management",
      .help_text = "View or change the time of day",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto& time = ctx.state.time();
        return CommandResult::success().add_message(compat::format(
            "{:02d}:{:02d}", time.current_hour(), time.current_minute()));
      },
  }
      .add_subcommand(make_game_time_set());
}

static CommandDef make_game_competition_monthly() {
  return CommandDef{
      .name = "monthly",
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
        ctx.state.competitions().run_monthly_competition();
        return CommandResult::success().add_message(
            "Monthly pizza competition triggered");
      },
  };
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
        core::events::MilestoneId message;
        if (cause_str == "killed") {
          cause = GameOverCause::KilledByMafia;
          message =
              core::events::MilestoneId::CoolVinnieAndBigLuigiTakeYouForABath;
        }
        else if (cause_str == "tycoon") {
          ctx.state.trigger_win_sequence(player->id());
          return CommandResult::success().add_message(
              "Game over triggered: tycoon");
        }
        else if (cause_str == "suppliers") {
          cause = GameOverCause::BankruptSuppliers;
          message = core::events::MilestoneId::NotAbleToPaySuppliers;
        }
        else {
          cause = GameOverCause::BankruptDebts;
          message = core::events::MilestoneId::NotAbleToPayDebts;
        }

        ctx.state.deliver_milestone(
            message, [player, cause, &state = ctx.state]() {
              state.eliminate_player(player->id(), cause);
            });
        return CommandResult::success().add_message(
            compat::format("Game over triggered: {}", cause_str));
      },
  };
}

static CommandDef make_game_levent() {
  return CommandDef{
      .name = "levent",
      .description = "Fire a life event (CV milestone) for the active player",
      .help_text = R"(Usage: game levent <id>

Delivers the given MilestoneId to the active player as if the milestone had
just been achieved: records the CV entry, awards score, and shows the
fullscreen message if the event has one. In mission mode this can trigger
the end-of-mission check.

Valid ids: 1-139 (see MilestoneId.h). 100+ are CV-only (no fullscreen).

Example:
  game levent 30    # "Opens 1st restaurant"
)",
      .args =
          {
              ArgSpec{
                  .name = "id",
                  .type = ArgType::Int,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "MilestoneId to fire (1-139)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* player = ctx.state.active_player();
        if (!player) {
          return CommandResult::error("No active player");
        }
        const int id = static_cast<int>(ctx.int_arg(0));
        if (id < 1 || id > 139) {
          return CommandResult::error("MilestoneId must be in 1-139");
        }
        const auto msg_id = static_cast<core::events::MilestoneId>(id);
        const bool already_shown =
            ctx.state.career_log().was_shown(msg_id, player->id());
        ctx.state.deliver_milestone(msg_id);
        return CommandResult::success().add_message(
            compat::format("Fired levent {}: {}{}", id,
                           ctx.state.career_log().cv_label(msg_id),
                           already_shown ? " (already shown)" : ""));
      },
  };
}

static CommandDef make_game_mission_write() {
  return CommandDef{
      .name = "write",
      .description = "Write the active player's CV as a mission file",
      .help_text = R"(Usage: game mission write <name>

Serializes the active player's career (CV) as a 400-byte .mis mission
file in the missions directory. The .mis extension is added if needed.

Example:
  game mission write mycareer
)",
      .args =
          {
              ArgSpec{
                  .name = "name",
                  .type = ArgType::String,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Mission filename (.mis added if needed)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* player = ctx.state.active_player();
        if (!player) {
          return CommandResult::error("No active player");
        }
        auto entries = ctx.state.career_log().get_cv_entries_for(player->id());
        if (entries.empty()) {
          return CommandResult::error("The active player's CV is empty");
        }
        std::string name = ctx.string_arg(0);
        if (name.ends_with(".mis")) {
          name = name.substr(0, name.size() - 4);
        }
        name = core::mission::sanitize_mission_name(name);
        if (name.empty()) {
          return CommandResult::error("Mission name is empty");
        }
        auto path = core::mission::save_career_mission(name, entries);
        if (!path) {
          return CommandResult::error(
              compat::format("Failed to write mission {}", name));
        }
        return CommandResult::success().add_message(compat::format(
            "Wrote {} milestones to {}", entries.size(), path->string()));
      },
  };
}

static CommandDef make_game_mission() {
  return CommandDef{
      .name = "mission",
      .description = "Mission mode target CV and scoring",
      .help_text = R"(Usage: game mission

Shows the mission's target milestones with the per-milestone match
percentage against the active player's CV, plus the overall score.
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* player = ctx.state.active_player();
        if (!player) {
          return CommandResult::error("No active player");
        }
        if (!ctx.state.is_mission_active()) {
          return CommandResult::error("No mission is active");
        }
        const auto& target = *ctx.state.mission_target();
        const auto live_cv =
            ctx.state.career_log().get_cv_entries_for(player->id());
        const auto scores =
            core::mission::per_milestone_scores(target, live_cv);

        auto result = CommandResult::success();
        result.add_message(compat::format("Mission: {} ({} milestones)",
                                          target.source_name,
                                          target.milestones.size()));
        for (size_t i = 0; i < target.milestones.size(); ++i) {
          const auto& milestone = target.milestones[i];
          result.add_message(compat::format(
              "  {:3}%  {:c}  [{}] {}", scores[i], milestone.date,
              static_cast<int>(milestone.message_id),
              ctx.state.career_log().cv_label(milestone.message_id)));
        }
        result.add_message(compat::format(
            "Overall: {}%", core::mission::overall_score(target, live_cv)));
        return result;
      },
  }
      .add_subcommand(make_game_mission_write());
}

static CommandDef make_game_competition() {
  return CommandDef{
      .name = "competition",
      .description = "Pizza competition commands",
      .help_text = "Trigger pizza competitions for testing",
  }
      .add_subcommand(make_game_competition_monthly());
}

static CommandDef make_game_timescale() {
  return CommandDef{
      .name = "timescale",
      .description = "Show or set the fast-forward multiplier",
      .help_text = R"(Usage: game timescale [n]

Runs simulation and animations at n times normal speed. Without an
argument, shows the current multiplier.

Example:
  game timescale 5
)",
      .args =
          {
              ArgSpec{
                  .name = "scale",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Speed multiplier (1-10)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        if (ctx.args.empty()) {
          return CommandResult::success().add_message(
              compat::format("Time scale is {}x", ctx.state.time_scale()));
        }
        const int scale = static_cast<int>(ctx.int_arg(0));
        if (scale < 1 || scale > 10) {
          return CommandResult::error("Scale must be in 1-10");
        }
        ctx.state.time_scale(scale);
        return CommandResult::success().add_message(
            compat::format("Time scale set to {}x", scale));
      },
  };
}

CommandDef make_game_commands() {
  return CommandDef{
      .name = "game",
      .description = "Game state management",
      .help_text = "Manage game state: save, load, exit",
  }
      .add_subcommand(make_game_save())
      .add_subcommand(make_game_load())
      .add_subcommand(make_game_exit())
      .add_subcommand(make_game_date())
      .add_subcommand(make_game_time())
      .add_subcommand(make_game_competition())
      .add_subcommand(make_game_end())
      .add_subcommand(make_game_levent())
      .add_subcommand(make_game_mission())
      .add_subcommand(make_game_timescale());
}

} // namespace debug::commands
