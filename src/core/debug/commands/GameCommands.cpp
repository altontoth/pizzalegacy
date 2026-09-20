/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "GameCommands.h"
#include "core/events/CareerLog.h"
#include "core/events/MilestoneId.h"
#include "core/food/PizzaJudging.h"
#include "core/mission/MissionFile.h"
#include "core/mission/MissionScoring.h"
#include "core/mission/MissionTarget.h"
#include "core/save/SaveGameManager.h"
#include "core/state/ClockRate.h"
#include "core/state/EndOfDayCause.h"
#include "core/state/GameOptions.h"
#include "core/state/GameRules.h"
#include "core/state/GameState.h"
#include "core/state/Overloaded.h"
#include "core/state/UIState.h"
#include "core/time/GameDate.h"
#include "core/time/GameTime.h"
#include "core/util/GameContext.h"
#include "engine/DisplayConstants.h"
#include "engine/FormatCompat.h"
#include "engine/audio/AudioManager.h"
#include "engine/input/InputManager.h"
#include "engine/ui/ICursorManager.h"

#include <charconv>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

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

struct OptionDef {
  std::string_view name;
  std::variant<bool GameOptions::*, int GameOptions::*, float GameOptions::*,
               FpsOption GameOptions::*, AutosaveFrequency GameOptions::*,
               bool GameRules::*, WinCondition GameRules::*,
               PizzaJudging GameRules::*>
      member;
  std::string_view description;
  // Inclusive bounds, for the numeric options only.
  double lowest = 0;
  double highest = 0;
  // Setting the field is not the whole job for these: the audio, display or
  // cursor has to be told as well, which this does the way MenuOptions does.
  void (*apply)(GameState&, GameContext&) = nullptr;
};

static const OptionDef option_defs[] = {
    {.name = "fix_legacy_bugs",
     .member = &GameRules::fix_legacy_bugs,
     .description = "Correct the original's bugs rather than reproduce them"},
    {.name = "qol_improvements",
     .member = &GameRules::qol_improvements,
     .description = "Conveniences the original did without"},
    {.name = "automatic_report",
     .member = &GameOptions::automatic_report,
     .description = "Show the day's turnover report when the turn ends"},
    {.name = "mafia_shakedowns_target_all_restaurants",
     .member = &GameRules::mafia_shakedowns_target_all_restaurants,
     .description = "Shake down a random restaurant instead of always the "
                    "first"},
    {.name = "demand_fulfillment_popularity",
     .member = &GameRules::demand_fulfillment_popularity,
     .description = "Fix bug in order fulfillment popularity"},
    {.name = "win_condition",
     .member = &GameRules::win_condition,
     .description = "Cities to dominate to win: paris, any_city or "
                    "all_cities"},
    {.name = "pizza_judging",
     .member = &GameRules::pizza_judging,
     .description = "How a standard recipe is judged: original or relaxed"},
    {.name = "attack_scene_hz",
     .member = &GameOptions::attack_scene_hz,
     .description = "Attack scene updates per second, 70 or 35 in the original",
     .lowest = GameOptions::ATTACK_SCENE_HZ_MIN,
     .highest = GameOptions::ATTACK_SCENE_HZ_MAX},
    {.name = "police",
     .member = &GameRules::police,
     .description = "Enable and disable police"},
    {.name = "show_advanced_options",
     .member = &GameOptions::show_advanced_options,
     .description = "Show the advanced rows on the options screens"},
    {.name = "autosave_frequency",
     .member = &GameOptions::autosave_frequency,
     .description = "off, daily, weekly or monthly"},
    {.name = "autosave_count",
     .member = &GameOptions::autosave_count,
     .description = "Rotating autosave slots",
     .lowest = GameOptions::AUTOSAVE_COUNT_MIN,
     .highest = GameOptions::AUTOSAVE_COUNT_MAX},
    {.name = "load_list_show_autosaves",
     .member = &GameOptions::load_list_show_autosaves,
     .description = "List the autosave slots on the game load screen"},
    {.name = "show_player_color_in_dropdown",
     .member = &GameOptions::show_player_color_in_dropdown,
     .description = "Mark players in the city map dropdowns with their "
                    "control map color"},
    {.name = "load_list_sort_by_date",
     .member = &GameOptions::load_list_sort_by_date,
     .description = "Sort the game load screen by date rather than name"},
    {.name = "map_fx",
     .member = &GameOptions::map_fx,
     .description = "Sound effects on the city map"},
    {.name = "restaurant_fx",
     .member = &GameOptions::restaurant_fx,
     .description = "Sound effects in the restaurant"},
    {.name = "music",
     .member = &GameOptions::music,
     .description = "Play music"},
    {.name = "sound_volume",
     .member = &GameOptions::sound_volume,
     .description = "Effects volume, 0 to 1",
     .lowest = 0.0,
     .highest = 1.0,
     .apply =
         [](GameState& state, GameContext& context) {
           context.audio->sound_volume(state.options().sound_volume);
         }},
    {.name = "music_volume",
     .member = &GameOptions::music_volume,
     .description = "Music volume, 0 to 1",
     .lowest = 0.0,
     .highest = 1.0,
     .apply =
         [](GameState& state, GameContext& context) {
           context.audio->music_volume(state.options().music_volume);
         }},
    {.name = "crt_aspect_ratio",
     .member = &GameOptions::crt_aspect_ratio,
     .description = "Show 640x480 (4:3) rather than 640x400",
     .apply =
         [](GameState& state, GameContext&) {
           state.request_crt_change(state.options().crt_aspect_ratio);
         }},
    {.name = "double_size",
     .member = &GameOptions::double_size,
     .description = "Start the window at twice the 640x400 surface",
     .apply =
         [](GameState& state, GameContext&) {
           state.request_double_size_change(state.options().double_size);
         }},
    {.name = "fullscreen",
     .member = &GameOptions::fullscreen,
     .description = "Fullscreen, the same toggle as Ctrl+F",
     .apply =
         [](GameState& state, GameContext&) {
           state.request_fullscreen_change(state.options().fullscreen);
         }},
    {.name = "render_fps",
     .member = &GameOptions::render_fps,
     .description = "30, 60 or 120; the simulation stays at 30Hz"},
    {.name = "vsync", .member = &GameOptions::vsync, .description = "Vsync"},
    {.name = "software_cursor",
     .member = &GameOptions::software_cursor,
     .description = "Draw the cursor rather than letting the system do it",
     .apply =
         [](GameState& state, GameContext& context) {
           context.cursor->software_mode(state.options().software_cursor);
         }},
    {.name = "key_repeat_delay_ms",
     .member = &GameOptions::key_repeat_delay_ms,
     .description = "How long a held key waits before it repeats",
     .lowest = GameOptions::KEY_REPEAT_DELAY_MIN,
     .highest = GameOptions::KEY_REPEAT_DELAY_MAX,
     .apply =
         [](GameState& state, GameContext& context) {
           context.input->key_repeat(state.options().key_repeat_delay_ms,
                                     state.options().key_repeat_interval_ms);
         }},
    {.name = "key_repeat_interval_ms",
     .member = &GameOptions::key_repeat_interval_ms,
     .description = "The gap between repeats of a held key",
     .lowest = GameOptions::KEY_REPEAT_INTERVAL_MIN,
     .highest = GameOptions::KEY_REPEAT_INTERVAL_MAX,
     .apply =
         [](GameState& state, GameContext& context) {
           context.input->key_repeat(state.options().key_repeat_delay_ms,
                                     state.options().key_repeat_interval_ms);
         }},
};

static const OptionDef* find_option(std::string_view name) {
  for (const auto& option : option_defs) {
    if (option.name == name) {
      return &option;
    }
  }
  return nullptr;
}

static std::string option_value(const GameOptions& options,
                                const GameRules& rules,
                                const OptionDef& option) {
  return std::visit(
      overloaded{[&](bool GameRules::* member) -> std::string {
                   return rules.*member ? "on" : "off";
                 },
                 [&](WinCondition GameRules::* member) -> std::string {
                   return std::string(to_config_value(rules.*member));
                 },
                 [&](PizzaJudging GameRules::* member) -> std::string {
                   return std::string(to_config_value(rules.*member));
                 },
                 [&](bool GameOptions::* member) -> std::string {
                   return options.*member ? "on" : "off";
                 },
                 [&](int GameOptions::* member) -> std::string {
                   return std::to_string(options.*member);
                 },
                 [&](float GameOptions::* member) -> std::string {
                   return compat::format("{:.2f}", options.*member);
                 },
                 [&](FpsOption GameOptions::* member) -> std::string {
                   return std::string(to_config_value(options.*member));
                 },
                 [&](AutosaveFrequency GameOptions::* member) -> std::string {
                   return std::string(to_config_value(options.*member));
                 }},
      option.member);
}

// Returns what went wrong, or nothing when the option was set.
static std::optional<std::string> set_option(GameOptions& options,
                                             GameRules& rules,
                                             const OptionDef& option,
                                             const std::string& value) {
  auto out_of_range = [&](double v) {
    return compat::format("{} takes {} to {}, got '{}'", option.name,
                          option.lowest, option.highest, value);
  };
  std::optional<std::string> failure;
  auto parse_bool = [&](bool& target) {
    if (value == "on" || value == "true" || value == "1") {
      target = true;
    }
    else if (value == "off" || value == "false" || value == "0") {
      target = false;
    }
    else {
      failure = compat::format("Expected on or off, got '{}'", value);
    }
  };
  std::visit(
      overloaded{
          [&](bool GameOptions::* member) { parse_bool(options.*member); },
          [&](bool GameRules::* member) { parse_bool(rules.*member); },
          [&](int GameOptions::* member) {
            int parsed = 0;
            auto [ptr, ec] = std::from_chars(
                value.data(), value.data() + value.size(), parsed);
            if (ec != std::errc{} || ptr != value.data() + value.size()) {
              failure = compat::format("Expected a number, got '{}'", value);
            }
            else if (parsed < option.lowest || parsed > option.highest) {
              failure = out_of_range(parsed);
            }
            else {
              options.*member = parsed;
            }
          },
          [&](float GameOptions::* member) {
            try {
              size_t consumed = 0;
              float parsed = std::stof(value, &consumed);
              if (consumed != value.size()) {
                failure = compat::format("Expected a number, got '{}'", value);
              }
              else if (parsed < option.lowest || parsed > option.highest) {
                failure = out_of_range(parsed);
              }
              else {
                options.*member = parsed;
              }
            }
            catch (const std::exception&) {
              failure = compat::format("Expected a number, got '{}'", value);
            }
          },
          [&](WinCondition GameRules::* member) {
            if (auto parsed = win_condition_from_config_value(value)) {
              rules.*member = *parsed;
            }
            else {
              failure = compat::format(
                  "Expected paris, any_city or all_cities, got '{}'", value);
            }
          },
          [&](PizzaJudging GameRules::* member) {
            if (auto parsed = pizza_judging_from_config_value(value)) {
              rules.*member = *parsed;
            }
            else {
              failure = compat::format("Expected original or relaxed, got '{}'",
                                       value);
            }
          },
          [&](FpsOption GameOptions::* member) {
            if (auto parsed = fps_option_from_config_value(value)) {
              options.*member = *parsed;
            }
            else {
              failure =
                  compat::format("Expected 30, 60 or 120, got '{}'", value);
            }
          },
          [&](AutosaveFrequency GameOptions::* member) {
            if (auto parsed = autosave_frequency_from_config_value(value)) {
              options.*member = *parsed;
            }
            else {
              failure = compat::format(
                  "Expected off, daily, weekly or monthly, got '{}'", value);
            }
          }},
      option.member);
  return failure;
}

static CommandDef make_game_options_set() {
  return CommandDef{
      .name = "set",
      .description = "Change an option for this run",
      .help_text = R"(Usage: game options set <name> <value>

Changes the option for this run only; the config file is left alone. Values
are spelled the way the config file spells them, which `game options` shows.

Examples:
  game options set fix_legacy_bugs off
  game options set win_condition all_cities
  game options set render_fps 120
)",
      .args =
          {
              ArgSpec{
                  .name = "name",
                  .type = ArgType::String,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Option name, as listed by `game options`",
              },
              ArgSpec{
                  .name = "value",
                  .type = ArgType::String,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "New value for the option",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        std::string name = ctx.string_arg(0);
        std::string value = ctx.string_arg(1);

        const auto* option = find_option(name);
        if (!option) {
          return CommandResult::error(
              compat::format("Unknown option '{}'", name));
        }

        auto& options = ctx.state.options();
        auto& rules = ctx.state.rules();
        if (auto failure = set_option(options, rules, *option, value)) {
          return CommandResult::error(*failure);
        }
        if (option->apply) {
          option->apply(ctx.state, ctx.context);
        }
        return CommandResult::success().add_message(compat::format(
            "{} is now {}", name, option_value(options, rules, *option)));
      },
  };
}

static CommandDef make_game_options() {
  return CommandDef{
      .name = "options",
      .description = "Show the game options",
      .help_text = R"(Usage: game options

Lists the options and their current values. `game options set` changes one
for this run.

Example:
  game options
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        const auto& options = ctx.state.options();
        const auto& rules = ctx.state.rules();
        auto result = CommandResult::success();
        result.add_message("Options:");
        for (const auto& option : option_defs) {
          result.add_message(compat::format(
              "  {:<40} {:<10}  {}", option.name,
              option_value(options, rules, option), option.description));
        }
        return result;
      },
  }
      .add_subcommand(make_game_options_set());
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

static std::string_view clock_rate_name(ClockRate rate) {
  switch (rate) {
  case ClockRate::Stopped:
    return "stopped";
  case ClockRate::SecondsOnly:
    return "seconds only";
  case ClockRate::Running:
    return "running";
  }
  return "unknown";
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

static CommandDef make_game_clock() {
  return CommandDef{
      .name = "clock",
      .description = "Clock state",
      .help_text = R"(Reports the rate the current screen asks for, whether
the turn pipeline has the clock suspended, and how far into the ten-minute
block the second counter has come.
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto& time = ctx.state.time();
        return CommandResult::success().add_message(compat::format(
            "{:02d}:{:02d}, clock {}{}, second {} of {} in the block",
            time.current_hour(), time.current_minute(),
            clock_rate_name(ctx.state.clock_rate()),
            ctx.state.is_clock_suspended() ? " (suspended)" : "",
            ctx.state.second_in_block(), seconds_per_block));
      },
  };
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

        if (!ctx.has_arg(0)) {
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
          if (!ctx.state.trigger_win_sequence(player->id())) {
            return CommandResult::error("This player has already won");
          }
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
      .help_text = compat::format(R"(Usage: game levent <id>

Delivers the given MilestoneId to the active player as if the milestone had
just been achieved: records the CV entry, awards score, and shows the
fullscreen message if the event has one. In mission mode this can trigger
the end-of-mission check.

Valid ids: 1-{} (see MilestoneId.h). 100+ are CV-only (no fullscreen).

Example:
  game levent 30    # "Opens 1st restaurant"
)",
                                  core::events::max_milestone_id),
      .args =
          {
              ArgSpec{
                  .name = "id",
                  .type = ArgType::Int,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = compat::format("MilestoneId to fire (1-{})",
                                                core::events::max_milestone_id),
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* player = ctx.state.active_player();
        if (!player) {
          return CommandResult::error("No active player");
        }
        const int id = static_cast<int>(ctx.int_arg(0));
        if (id < 1 || id > core::events::max_milestone_id) {
          return CommandResult::error(compat::format(
              "MilestoneId must be in 1-{}", core::events::max_milestone_id));
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
        if (!ctx.has_arg(0)) {
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

static CommandDef make_game_realtime() {
  return CommandDef{
      .name = "realtime",
      .description = "Run the clock as if the player were on a live screen",
      .help_text = R"(Usage: game realtime <on|off>

The clock only runs, and the day only ends by itself, while the player is on
the city map or in a restaurant. A script reaches no such screen, so it has to
turn this on before time will pass.

Example:
  game realtime on
)",
      .args =
          {
              ArgSpec{
                  .name = "mode",
                  .type = ArgType::String,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "on or off",
                  .allowed_values = std::vector<std::string>{"on", "off"},
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        const bool on = ctx.string_arg(0) == "on";
        if (on) {
          ctx.state.enter_realtime_play();
        }
        else {
          ctx.state.leave_realtime_play();
        }
        return CommandResult::success().add_message(
            compat::format("Real-time play {}", on ? "on" : "off"));
      },
  };
}

static CommandDef make_game_turn_end() {
  return CommandDef{
      .name = "end",
      .description = "End the active player's turn",
      .help_text = R"(Usage: game turn end

Ends the active player's turn on the next simulation tick, the same way the
End turn button does. Run `game tick` afterwards to let the turn boundary
play out.
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        if (!ctx.state.active_player()) {
          return CommandResult::error("No active player");
        }
        ctx.state.turn().request_end_of_turn(EndOfDayCause::PlayerAction);
        return CommandResult::success().add_message("End of turn requested");
      },
  };
}

static CommandDef make_game_turn_queue() {
  return CommandDef{
      .name = "queue",
      .description = "List the mayor's and police's queued calls",
      .help_text = R"(Usage: game turn queue

Lists the mayor's and police's deferred calls in the order they were queued,
with the player each one belongs to.
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        using Kind = TurnOrchestrator::DeferredCall::Kind;
        const auto& queued = ctx.state.turn().deferred_calls();
        if (queued.empty()) {
          return CommandResult::success().add_message("Queue empty");
        }
        auto result = CommandResult::success();
        for (const auto& ev : queued) {
          switch (ev.kind) {
          case Kind::MayorInspection:
            result.add_message(compat::format(
                "player {}: inspection of restaurant {} at block {}",
                ev.player_id.value, ev.restaurant_id.value, ev.display_block));
            break;
          case Kind::MayorAward:
            result.add_message(compat::format(
                "player {}: award {} ({})", ev.player_id.value,
                static_cast<int>(ev.award.type),
                ev.award.announces_winner ? "winner" : "advance notice"));
            break;
          case Kind::DepotRaid:
            result.add_message(
                compat::format("player {}: depot raid", ev.player_id.value));
            break;
          }
        }
        return result;
      },
  };
}

static CommandDef make_game_turn() {
  return CommandDef{
      .name = "turn",
      .description = "Turn control",
      .help_text = "End the active player's turn",
  }
      .add_subcommand(make_game_turn_end())
      .add_subcommand(make_game_turn_queue());
}

static CommandDef make_game_tick() {
  return CommandDef{
      .name = "tick",
      .description = "Advance the simulation by whole ticks",
      .help_text = R"(Usage: game tick [n]

Runs n simulation steps, the same step the game loop runs 30 times a second.
This is what lets a script get anywhere that needs time to pass: the turn
boundary, a day rollover, the clock reaching the player's end of day.

Example:
  game tick 100
)",
      .args =
          {
              ArgSpec{
                  .name = "count",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Number of ticks (default 1)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        const int count = ctx.has_arg(0) ? static_cast<int>(ctx.int_arg(0)) : 1;
        if (count < 1) {
          return CommandResult::error("Tick count must be at least 1");
        }
        for (int i = 0; i < count; ++i) {
          ctx.state.update(simulation_tick_ms);
        }
        return CommandResult::success().add_message(compat::format(
            "Ran {} tick(s); now {}", count, ctx.state.time().to_string()));
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
      .add_subcommand(make_game_options())
      .add_subcommand(make_game_time())
      .add_subcommand(make_game_clock())
      .add_subcommand(make_game_competition())
      .add_subcommand(make_game_end())
      .add_subcommand(make_game_levent())
      .add_subcommand(make_game_mission())
      .add_subcommand(make_game_timescale())
      .add_subcommand(make_game_realtime())
      .add_subcommand(make_game_turn())
      .add_subcommand(make_game_tick());
}

} // namespace debug::commands
