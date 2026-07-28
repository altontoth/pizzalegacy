/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/commands/CityCommands.h"

#include <algorithm>
#include <string>
#include "core/character/AiMenuSlot.h"
#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/PizzaCharts.h"
#include "core/food/PlayerRecipes.h"
#include "core/food/Recipe.h"
#include "core/state/GameState.h"
#include "core/util/Constants.h"
#include "engine/FormatCompat.h"

using namespace debug::command;

namespace debug::commands {

// Show the current pizza chart (top 10 from the rendered view)
static CommandDef make_chart_show() {
  return CommandDef{
      .name = "show",
      .description = "Show current pizza chart (top 10)",
      .help_text = R"(Usage: city chart show

Shows the current pizza chart exactly as rendered in Market Research.

Example:
  city chart show
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }

        const auto& entries = city->pizza_charts().entries();
        if (entries.empty()) {
          return CommandResult::success().add_message(
              "Pizza chart is empty (advance a day to populate)");
        }

        CommandResult result = CommandResult::success();
        result.add_message(compat::format(
            "Pizza Charts - {} ({} entries):", city->name(), entries.size()));
        result.add_message(" #  Prev  Score  Pizza                Player");
        result.add_message(
            "--- ----  -----  -------------------  ----------------");

        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
          const auto& e = entries[i];
          std::string prev = e.previous_position == 0
                                 ? " -"
                                 : compat::format("{:>2}", e.previous_position);
          result.add_message(compat::format("{:>2}. {:>4}  {:>5}  {:<19}  {}",
                                            i + 1, prev, e.score, e.pizza_name,
                                            e.player_name));
        }
        return result;
      },
  };
}

// Rebuild chart and show all candidates (not just top 10) for debugging
static CommandDef make_chart_debug() {
  return CommandDef{
      .name = "debug",
      .description = "Show ALL chart candidates with scores",
      .help_text =
          R"(Usage: city chart debug [player_id]

Shows all scored pizza entries before the top-10 cut, grouped by player.
Optionally filter to a single player.

Examples:
  city chart debug        # All players
  city chart debug @      # Active player only
  city chart debug 0      # Player 0 only
)",
      .args =
          {
              ArgSpec{
                  .name = "player_id",
                  .type = ArgType::PlayerId,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Filter to specific player (@ for active)",
                  .min_value = 0,
                  .max_value = NUMBER_OF_PLAYERS - 1,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }

        // Determine filter
        bool filter_player = !ctx.args.empty();
        core::PlayerId filter_id{0};
        if (filter_player) {
          filter_id = ctx.player_id(0);
        }

        CommandResult result = CommandResult::success();
        result.add_message(
            compat::format("Pizza Chart Debug - {}", city->name()));
        result.add_message("");

        auto players = ctx.state.players();
        for (auto* player : players) {
          if (!player) {
            continue;
          }
          if (filter_player && player->id() != filter_id) {
            continue;
          }

          std::string type_str = player->is_human_controlled() ? "Human" : "AI";
          int rest_count =
              static_cast<int>(ctx.state.restaurants_for(player->id()).size());
          result.add_message(compat::format(
              "Player {} ({}) - {} [{}] - {} restaurant(s):",
              player->id().value, type_str, player->name(),
              player->is_human_controlled()
                  ? compat::format("{} recipes", player->recipe_count())
                  : compat::format("cooking={}", player->character().cooking()),
              rest_count));

          if (player->is_human_controlled()) {
            auto recipes = player->recipes().all();
            std::ranges::sort(recipes, [](const auto* a, const auto* b) {
              return a->chart_score() > b->chart_score();
            });
            if (recipes.empty()) {
              result.add_message("  (no recipes)");
            }
            for (const auto* recipe : recipes) {
              std::string days;
              for (int sold : recipe->daily_sales()) {
                days += compat::format("{:>5}", sold);
              }
              result.add_message(compat::format("  Score {:>5} [{} ]  {}",
                                                recipe->chart_score(), days,
                                                recipe->display_name()));
            }
          }
          else {
            // Show AI menu slots
            const auto& slots = player->ai_menu_slots();
            bool any = false;
            for (int i = 0; i < 10; ++i) {
              const auto& slot = slots[i];
              if (slot.recipe_id == 0) {
                continue;
              }
              any = true;
              result.add_message(
                  compat::format("  Slot {:>2}: recipe_id={:>3}  prod={:>3}  "
                                 "max_prod={:>3}  chart_score={:>5}",
                                 i, slot.recipe_id, slot.production_counter,
                                 slot.max_production, slot.chart_score()));
            }
            if (!any) {
              result.add_message("  (no active menu slots)");
            }
          }
          result.add_message("");
        }

        return result;
      },
  };
}

// Force rebuild the chart (without waiting for end_of_day)
static CommandDef make_chart_rebuild() {
  return CommandDef{
      .name = "rebuild",
      .description = "Force rebuild the pizza chart now",
      .help_text = R"(Usage: city chart rebuild

Rebuilds the pizza chart immediately without waiting for end of day.

Example:
  city chart rebuild
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }

        city->pizza_charts().build(ctx.state);
        int count = static_cast<int>(city->pizza_charts().entries().size());
        return CommandResult::success().add_message(
            compat::format("Chart rebuilt: {} entries", count));
      },
  };
}

static CommandDef make_chart() {
  return CommandDef{
      .name = "chart",
      .aliases = {"charts"},
      .description = "Pizza chart commands",
      .help_text = "Subcommands: show, debug, rebuild",
  }
      .add_subcommand(make_chart_show())
      .add_subcommand(make_chart_debug())
      .add_subcommand(make_chart_rebuild());
}

CommandDef make_city_commands() {
  return CommandDef{
      .name = "city",
      .description = "City-level commands",
      .help_text = "City management and diagnostics",
  }
      .add_subcommand(make_chart());
}

} // namespace debug::commands
