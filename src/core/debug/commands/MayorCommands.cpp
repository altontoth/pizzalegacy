/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/commands/MayorCommands.h"

#include "core/city/City.h"
#include "core/city/MayorOffice.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"

using namespace debug::command;

namespace debug::commands {

static std::optional<MayorAwardType> parse_award_type(const std::string& s) {
  if (s == "beauty") {
    return MayorAwardType::MostBeautifulRestaurant;
  }
  if (s == "pizza") {
    return MayorAwardType::PizzaOfTheYear;
  }
  if (s == "investor") {
    return MayorAwardType::BestInvestorInTown;
  }
  if (s == "imitator") {
    return MayorAwardType::BestImitator;
  }
  if (s == "bowtie") {
    return MayorAwardType::GoldenBowTie;
  }
  if (s == "trade") {
    return MayorAwardType::TransAtlanticTrade;
  }
  return std::nullopt;
}

static CommandDef make_mayor_inspect() {
  return CommandDef{
      .name = "inspect",
      .description = "Queue and trigger a mayor restaurant inspection",
      .help_text = R"(Usage: mayor inspect [restaurant_id] [first]

Forces a mayor quality inspection for the given restaurant.
Defaults to the active restaurant. Whether it is a first-time inspection
is inferred from the restaurant's state; pass true/false to override.

Examples:
  mayor inspect           # Inspect active restaurant (auto first/regular)
  mayor inspect 2         # Inspect restaurant 2 (auto)
  mayor inspect 2 true    # Force first-time inspection for restaurant 2
  mayor inspect 2 false   # Force regular inspection for restaurant 2
)",
      .args =
          {
              ArgSpec{
                  .name = "restaurant_id",
                  .type = ArgType::RestaurantId,
                  .optional = true,
                  .default_provider = DefaultProvider::ActiveRestaurant,
                  .description = "Restaurant ID (@ for active)",
              },
              ArgSpec{
                  .name = "first",
                  .type = ArgType::Bool,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "true = first-time inspection (default false)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto rid = ctx.restaurant_id(0);

        auto* restaurant = ctx.state.restaurant(rid);
        if (!restaurant) {
          return CommandResult::error(
              compat::format("Restaurant {} not found", rid.value));
        }

        bool is_first = ctx.args.size() >= 2
                            ? ctx.bool_arg(1)
                            : restaurant->first_inspection_pending();

        ctx.state.turn().trigger_mayor_inspection(rid, is_first);
        return CommandResult::success().add_message(
            compat::format("Triggered {} inspection for restaurant {}",
                           is_first ? "first" : "regular", rid.value));
      },
  };
}

static CommandDef make_mayor_award() {
  return CommandDef{
      .name = "award",
      .description = "Trigger or queue a mayor award",
      .help_text = R"(Usage: mayor award <type> [winner_player_id] [when]

Has the mayor call to announce an award.
winner_player_id defaults to the active player. Pass -1 to name no winner, the
way the pizza competition announcement does. beauty and bowtie score their own
winner, so -1 will not stop them.

Types: beauty, pizza, investor, imitator, bowtie, trade

when is "now" for the mayor to call straight away, or "queue" to leave it
waiting the way the mayor's own scheduling does, for delivery on the player's
next turn.

Examples:
  mayor award beauty         # Beauty award for active player
  mayor award pizza          # Pizza-of-year for active player
  mayor award pizza -1       # Pizza competition announcement (no winner)
  mayor award investor 1     # Investor award for player 1
  mayor award beauty 0 queue # Leave it for the next turn
)",
      .args =
          {
              ArgSpec{
                  .name = "type",
                  .type = ArgType::String,
                  .optional = false,
                  .description = "Award type",
                  .allowed_values =
                      std::vector<std::string>{"beauty", "pizza", "investor",
                                               "imitator", "bowtie", "trade"},
              },
              ArgSpec{
                  .name = "winner_player_id",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Winner player ID (-1 for announcement only)",
                  .min_value = -1,
                  .max_value = NUMBER_OF_PLAYERS - 1,
              },
              ArgSpec{
                  .name = "when",
                  .type = ArgType::String,
                  .optional = true,
                  .description = "now or queue (default: now)",
                  .allowed_values = std::vector<std::string>{"now", "queue"},
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto type_str = ctx.string_arg(0);
        auto award_type = parse_award_type(type_str);
        if (!award_type) {
          return CommandResult::error(
              compat::format("Unknown award type: {}", type_str));
        }

        std::optional<int> winner_id;
        if (ctx.args.size() >= 2) {
          int raw = static_cast<int>(ctx.int_arg(1));
          if (raw >= 0) {
            winner_id = raw;
          }
        }
        else {
          auto* player = ctx.state.active_player();
          if (player) {
            winner_id = player->id().value;
          }
        }

        MayorAwardEvent event{.type = *award_type,
                              .winner_player_id = winner_id,
                              .is_ceremony = true};

        const std::string winner =
            winner_id.has_value() ? std::to_string(*winner_id) : "none";
        if (ctx.args.size() > 2 && ctx.string_arg(2) == "queue") {
          ctx.state.turn().queue_mayor_award(event);
          return CommandResult::success().add_message(
              compat::format("Queued {} award (winner={})", type_str, winner));
        }

        ctx.state.process_mayor_award_event(event);
        return CommandResult::success().add_message(
            compat::format("Triggered {} award (winner={})", type_str, winner));
      },
  };
}

static CommandDef make_mayor_favor() {
  return CommandDef{
      .name = "favor",
      .description = "Set the mayor favor score for a player",
      .help_text = R"(Usage: mayor favor <amount> [player_id]

Directly sets the mayor favor score (0-200) for the player.
Defaults to the active player.

Examples:
  mayor favor 100       # Set active player's favor to 100
  mayor favor 200 1     # Max favor for player 1
  mayor favor 0         # Zero out active player's favor
)",
      .args =
          {
              ArgSpec{
                  .name = "amount",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "Favor score (0-200)",
                  .min_value = 0,
                  .max_value = 200,
              },
              ArgSpec{
                  .name = "player_id",
                  .type = ArgType::PlayerId,
                  .optional = true,
                  .default_provider = DefaultProvider::ActivePlayer,
                  .description = "Player ID (@ for active player)",
                  .min_value = 0,
                  .max_value = NUMBER_OF_PLAYERS - 1,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        int amount = static_cast<int>(ctx.int_arg(0));
        auto player_id = ctx.player_id(1);

        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }

        city->mayor_office().favor(player_id.value, amount);
        return CommandResult::success().add_message(compat::format(
            "Set mayor favor for player {} to {}", player_id.value, amount));
      },
  };
}

static CommandDef make_mayor_status() {
  return CommandDef{
      .name = "status",
      .description = "Show mayor office state",
      .help_text = R"(Usage: mayor status

Shows the mayor's two bank account numbers, favor scores for all
players, appointment day, and quarterly award schedule for the
active city.

Example:
  mayor status
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }

        auto& mo = city->mayor_office();
        auto result = CommandResult::success();

        result.add_message(compat::format("Private (donation) account: {}",
                                          mo.private_bank_account()));
        result.add_message(compat::format("Public (award fee) account: {}",
                                          mo.public_bank_account()));

        auto players = ctx.state.players();
        for (int i = 0; i < static_cast<int>(players.size()); ++i) {
          if (players[i]) {
            result.add_message(
                compat::format("Player {} favor: {}", i, mo.favor_score(i)));
          }
        }

        result.add_message(
            compat::format("Appointment day: {}", mo.appointment_day()));

        return result;
      },
  };
}

CommandDef make_mayor_commands() {
  return CommandDef{
      .name = "mayor",
      .description = "Mayor interaction commands",
      .help_text = "Trigger mayor events (inspect, award, favor, status)",
  }
      .add_subcommand(make_mayor_inspect())
      .add_subcommand(make_mayor_award())
      .add_subcommand(make_mayor_favor())
      .add_subcommand(make_mayor_status());
}

} // namespace debug::commands
