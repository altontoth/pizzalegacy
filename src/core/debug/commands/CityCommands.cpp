/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/commands/CityCommands.h"

#include <algorithm>
#include <string>
#include <vector>
#include "core/character/Actor.h"
#include "core/character/AiMenuSlot.h"
#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/District.h"
#include "core/city/PizzaCharts.h"
#include "core/city/Property.h"
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
          const Player* owner = ctx.state.player(e.player_id);
          if (!owner) {
            continue;
          }
          std::string prev = e.previous_position == 0
                                 ? " -"
                                 : compat::format("{:>2}", e.previous_position);
          result.add_message(compat::format(
              "{:>2}. {:>4}  {:>5}  {:<19}  {}", i + 1, prev, e.score,
              PizzaCharts::pizza_name(*owner, e.pizza_ref), owner->name()));
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
            auto slots = player->ai_menu_slots();
            std::ranges::sort(slots, [](const auto& a, const auto& b) {
              return a.chart_score() > b.chart_score();
            });
            bool any = false;
            for (const auto& slot : slots) {
              if (slot.pizza_id == ai_pizza_none) {
                continue;
              }
              any = true;
              std::string days;
              for (int produced : slot.history) {
                days += compat::format("{:>5}", produced);
              }
              result.add_message(
                  compat::format("  Score {:>5} [{} ]  {} (ref {}, prod {}/{})",
                                 slot.chart_score(), days,
                                 PizzaCharts::pizza_name(
                                     *player, PizzaChartRef::of(slot.pizza_id)),
                                 slot.pizza_id.value, slot.production_counter,
                                 slot.max_production));
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

// Premise price, rent and broker -- what the realtor negotiates over.
static CommandDef make_property() {
  return CommandDef{
      .name = "property",
      .aliases = {"properties"},
      .description = "Show premises with their price, rent and realtor",
      .help_text = R"(Usage: city property [index]

With no argument, lists every premise that is for sale or for rent. With an
index, shows that one premise whatever its status.

Examples:
  city property
  city property 4213
)",
      .args =
          {
              ArgSpec{
                  .name = "index",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Map index of a single premise",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }

        std::vector<Property*> shown;
        if (ctx.args.empty()) {
          for (auto* property : city->properties()) {
            if (property->is_for_sale() || property->is_for_rent()) {
              shown.push_back(property);
            }
          }
        }
        else {
          auto index = static_cast<int>(ctx.int_arg(0));
          auto* property = city->property_by_index(index);
          if (!property) {
            return CommandResult::error(
                compat::format("No premise at index {}", index));
          }
          shown.push_back(property);
        }

        CommandResult result = CommandResult::success();
        result.add_message(
            compat::format("{} premises in {}", shown.size(), city->name()));
        for (auto* property : shown) {
          const auto* realtor = city->actor_by_id(property->realtor_id());
          const auto& pool = property->potential_customers().data();
          result.add_message(compat::format(
              "  {:5} {:2}x{:<2} price {:i} rent {:i} realtor {} ({}) "
              "customers {} {} {} {} {} {}",
              property->index(), property->size().w, property->size().h,
              property->purchase_price(), property->get_monthly_rent(),
              property->realtor_id(), realtor ? realtor->name() : "none",
              pool[0], pool[1], pool[2], pool[3], pool[4], pool[5]));
        }
        return result;
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

static CommandDef make_districts() {
  return CommandDef{
      .name = "districts",
      .description = "List the active city's districts",
      .help_text = R"(Usage: city districts [city_id]

Lists each district with its position and the id the city file gives it.
Loads the city if it is not loaded yet.

Examples:
  city districts       # the active city
  city districts 3     # city 3
)",
      .args =
          {
              ArgSpec{
                  .name = "city_id",
                  .type = ArgType::CityId,
                  .optional = true,
                  .default_provider = DefaultProvider::ActiveCity,
                  .description = "City ID (@ for active city)",
                  .min_value = 0,
                  .max_value = 9,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto city_id = ctx.city_id(0);
        ctx.state.ensure_city_loaded(city_id);
        auto* city = ctx.state.city(city_id);
        if (!city) {
          return CommandResult::error(
              compat::format("City {} not found", city_id.value));
        }
        CommandResult result = CommandResult::success();
        result.add_message(compat::format("Districts of {}:", city->name()));
        result.add_message("Pos  Id  Name");
        result.add_message("---  --  ----------------");
        for (const auto& district : city->districts()) {
          result.add_message(compat::format("{:>3}  {:>2}  {}",
                                            district->index().value,
                                            district->id(), district->name()));
        }
        return result;
      },
  };
}

static CommandDef make_investigation() {
  return CommandDef{
      .name = "investigation",
      .description = "Set the district id the police are investigating",
      .help_text = R"(Usage: city investigation <district_id>

Sets the district id whose depots the police will raid next month. Ids come
from `city districts`; an invalid id produces no result.

Example:
  city investigation 4
)",
      .args =
          {
              ArgSpec{
                  .name = "district_id",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "District id to investigate",
                  .min_value = 0,
                  .max_value = 255,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        int id = static_cast<int>(ctx.int_arg(0));
        ctx.state.district_under_investigation(id);
        return CommandResult::success().add_message(
            compat::format("District under investigation set to {}", id));
      },
  };
}

CommandDef make_city_commands() {
  return CommandDef{
      .name = "city",
      .description = "City-level commands",
      .help_text = "City management and diagnostics",
  }
      .add_subcommand(make_chart())
      .add_subcommand(make_districts())
      .add_subcommand(make_investigation())
      .add_subcommand(make_property());
}

} // namespace debug::commands
