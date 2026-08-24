/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/commands/ActorCommands.h"

#include "conversations/PoliceConversation.h"
#include "core/GameTypes.h"
#include "core/character/Actor.h"
#include "core/character/MafiaPlayerState.h"
#include "core/character/Player.h"
#include "core/character/SocialStatus.h"
#include "core/city/City.h"
#include "core/city/CityNames.h"
#include "core/item/Item.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/util/Constants.h"
#include "engine/FormatCompat.h"

#include <algorithm>

using namespace debug::command;

namespace debug::commands {

static const char* mission_type_name(MafiaMissionType t) {
  switch (t) {
  case MafiaMissionType::Package:
    return "Package";
  case MafiaMissionType::Attack:
    return "Attack";
  case MafiaMissionType::Weapon:
    return "Weapon";
  case MafiaMissionType::Laundering:
    return "Laundering";
  case MafiaMissionType::Investment:
    return "Investment";
  }
  return "Unknown";
}

static std::string location_info(const MissionLocation& loc, const City* city) {
  std::string name = "(no city)";
  if (city && loc.tile_index >= 0) {
    name = city->building_name(loc.tile_index);
  }
  return compat::format("tile={} ({}) date={} {}:{:02}", loc.tile_index, name,
                        loc.date.to_string(), loc.hour, loc.minute);
}

static CommandDef make_mafia_shakedown() {
  return CommandDef{
      .name = "shakedown",
      .description = "Trigger a mafia protection racket visit",
      .help_text = R"(Usage: mafia shakedown [restaurant_id] [when]

Forces a mafia protection racket visit for the given restaurant.
Defaults to the active restaurant.

when is "now" to put the offer on screen, or "queue" to leave it waiting the
way the end of a turn does, for delivery on the player's next turn.

Examples:
  mafia shakedown              # Shakedown restaurant 0
  mafia shakedown 1            # Shakedown restaurant 1
  mafia shakedown 1 queue      # Leave it for the next turn
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
                  .name = "when",
                  .type = ArgType::String,
                  .optional = true,
                  .description = "now or queue (default: now)",
                  .allowed_values = std::vector<std::string>{"now", "queue"},
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }
        auto mafia_actors = city->actors(ActorType::MAFIA);
        if (mafia_actors.empty()) {
          return CommandResult::error("No mafia actor in this city");
        }
        int mafia_id = mafia_actors[0]->id();

        auto rid = ctx.restaurant_id(0);
        Restaurant* restaurant = ctx.state.restaurant(rid);
        if (!restaurant) {
          return CommandResult::error(
              compat::format("Restaurant {} not found", rid.value));
        }
        bool first = !restaurant->protection_contacted();
        restaurant->protection_timer(30);
        restaurant->protection_contacted(true);

        if (ctx.args.size() > 1 && ctx.string_arg(1) == "queue") {
          auto* player = ctx.state.active_player();
          ctx.state.turn().restore_deferred_racket_offer(
              {.player_id = player->id(),
               .restaurant_id = rid,
               .first_contact = first});
          return CommandResult::success().add_message(compat::format(
              "Queued a mafia shakedown for restaurant {}", rid.value));
        }

        ctx.state.ui().push<MafiaState>(
            MafiaState{.id = mafia_id,
                       .mode = MafiaMode::ProtectionRacketOffer,
                       .restaurant_id = rid,
                       .first_contact = first});
        return CommandResult::success().add_message(compat::format(
            "Triggered mafia shakedown for restaurant {}", rid.value));
      },
  };
}

static CommandDef make_mafia_loan_due() {
  return CommandDef{
      .name = "loan-due",
      .description = "Trigger mafia loan repayment event",
      .help_text = R"(Usage: actor mafia loan-due

Forces the mafia to come collect on a loan.
The player must have an active mafia loan.

Example:
  actor mafia loan-due
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* player = ctx.state.active_player();
        if (!player || !player->mafia_state().loan.has_value()) {
          return CommandResult::error("Player has no active mafia loan");
        }
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }
        auto mafia_actors = city->actors(ActorType::MAFIA);
        if (mafia_actors.empty()) {
          return CommandResult::error("No mafia actor in this city");
        }

        // The call comes from the clan that made the loan.
        int clan = std::clamp(player->mafia_state().loan->clan, 0,
                              static_cast<int>(mafia_actors.size()) - 1);
        ctx.state.ui().push<MafiaState>(
            MafiaState{.id = mafia_actors[clan]->id(),
                       .mode = MafiaMode::LoanRepaymentDue,
                       .restaurant_id = std::nullopt});
        return CommandResult::success().add_message(
            "Triggered mafia loan repayment");
      },
  };
}

static CommandDef make_mafia_mission_overdue() {
  return CommandDef{
      .name = "mission-overdue",
      .description = "Trigger mafia mission overdue event",
      .help_text = R"(Usage: actor mafia mission-overdue

Forces the mafia to punish the player for a failed mission.
The player must have an active mafia mission.

Example:
  actor mafia mission-overdue
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* player = ctx.state.active_player();
        if (!player || !player->mafia_state().active_mission.has_value()) {
          return CommandResult::error("Player has no active mafia mission");
        }
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }
        auto mafia_actors = city->actors(ActorType::MAFIA);
        if (mafia_actors.empty()) {
          return CommandResult::error("No mafia actor in this city");
        }

        // The call comes from the clan that set the mission.
        int clan = std::clamp(player->mafia_state().active_mission->clan, 0,
                              static_cast<int>(mafia_actors.size()) - 1);
        ctx.state.ui().push<MafiaState>(
            MafiaState{.id = mafia_actors[clan]->id(),
                       .mode = MafiaMode::MissionOverdue,
                       .restaurant_id = std::nullopt});
        return CommandResult::success().add_message(
            "Triggered mafia mission overdue");
      },
  };
}

static CommandDef make_mafia_status() {
  return CommandDef{
      .name = "status",
      .description = "Show mafia-related player state",
      .help_text = R"(Usage: actor mafia status

Shows underworld score, active missions, loans, and restaurant
protection timers.

Example:
  actor mafia status
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* player = ctx.state.active_player();
        if (!player) {
          return CommandResult::error("No active player");
        }
        auto result = CommandResult::success();
        auto& ss = player->social_status();
        int rank =
            social_rank(ss.underworld_score(), ctx.state.difficulty_level());
        result.add_message(compat::format("Underworld: score={} rank={}",
                                          ss.underworld_score(), rank));
        result.add_message(
            compat::format("Social: score={}", ss.social_score()));

        auto& ms = player->mafia_state();
        if (ms.loan) {
          result.add_message(compat::format("Loan: {} due {}", ms.loan->amount,
                                            ms.loan->due_date.to_string()));
        }
        else {
          result.add_message("Loan: none");
        }

        if (ms.active_mission) {
          auto& m = *ms.active_mission;
          auto* city = ctx.state.active_city();
          result.add_message(compat::format(
              "Mission: type={} active={} risk={}% reward={} busted={}",
              mission_type_name(m.mission_type), m.active, m.risk_pct, m.reward,
              m.busted));
          result.add_message(compat::format(
              "  deadline={} {}:{:02} completed={}", m.deadline.to_string(),
              m.deadline_hour, m.deadline_minute, m.completed));
          result.add_message(
              compat::format("  pickup: {}", location_info(m.pickup, city)));
          result.add_message(
              compat::format("  dropoff: {}", location_info(m.dropoff, city)));
        }
        else {
          result.add_message("Mission: none");
        }

        auto restaurants = ctx.state.restaurants_for(player->id());
        for (auto* r : restaurants) {
          auto t = r->protection_timer();
          std::string timer_str =
              t.has_value() ? compat::format("{}", *t) : "expired";
          result.add_message(compat::format(
              "Restaurant {}: protection_timer={} (1=eligible) contacted={}",
              r->id().value, timer_str, r->protection_contacted()));
        }

        return result;
      },
  };
}

static CommandDef make_mafia_set_bust() {
  return CommandDef{
      .name = "set-bust",
      .description = "Toggle the busted flag on the active mission",
      .help_text = R"(Usage: actor mafia set-bust <true|false>

Sets the busted flag on the active mafia mission for testing.

Examples:
  actor mafia set-bust true   # Will trigger police on pickup/dropoff
  actor mafia set-bust false  # Normal mission flow
)",
      .args =
          {
              ArgSpec{
                  .name = "busted",
                  .type = ArgType::Bool,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "true or false",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* player = ctx.state.active_player();
        if (!player || !player->mafia_state().active_mission.has_value()) {
          return CommandResult::error("Player has no active mafia mission");
        }
        bool val = ctx.bool_arg(0);
        player->mafia_state().active_mission->busted = val;
        return CommandResult::success().add_message(
            compat::format("Set busted={}", val));
      },
  };
}

static CommandDef make_mafia_set_pickup() {
  return CommandDef{
      .name = "set-pickup",
      .description = "Override pickup tile and time",
      .help_text = R"(Usage: actor mafia set-pickup <tile_index> [hour] [minute]

Sets the pickup location to the given tile index and optionally
overrides the time window to today at the specified hour:minute.
If hour/minute are omitted, uses the current game time.

Examples:
  actor mafia set-pickup 1         # Tile 1, current time
  actor mafia set-pickup 3 14 30   # Tile 3 at 14:30 today
)",
      .args =
          {
              ArgSpec{
                  .name = "tile_index",
                  .type = ArgType::Int,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Tile index",
              },
              ArgSpec{
                  .name = "hour",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Hour (0-23), default: current hour",
              },
              ArgSpec{
                  .name = "minute",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Minute (0-59), default: current minute",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* player = ctx.state.active_player();
        if (!player || !player->mafia_state().active_mission.has_value()) {
          return CommandResult::error("Player has no active mafia mission");
        }
        auto& pickup = player->mafia_state().active_mission->pickup;
        pickup.tile_index = static_cast<int>(ctx.int_arg(0));
        pickup.date = ctx.state.time().current_date();
        if (ctx.args.size() >= 2) {
          pickup.hour = static_cast<int>(ctx.int_arg(1));
        }
        else {
          pickup.hour = ctx.state.time().current_hour();
        }
        if (ctx.args.size() >= 3) {
          pickup.minute = static_cast<int>(ctx.int_arg(2));
        }
        else {
          pickup.minute = ctx.state.time().current_minute();
        }
        auto* city = ctx.state.active_city();
        return CommandResult::success().add_message(
            compat::format("Pickup: {}", location_info(pickup, city)));
      },
  };
}

static CommandDef make_mafia_set_dropoff() {
  return CommandDef{
      .name = "set-dropoff",
      .description = "Override dropoff tile and time",
      .help_text =
          R"(Usage: actor mafia set-dropoff <tile_index> [hour] [minute]

Sets the dropoff location to the given tile index and optionally
overrides the time window to today at the specified hour:minute.
If hour/minute are omitted, uses the current game time.

Examples:
  actor mafia set-dropoff 5         # Tile 5, current time
  actor mafia set-dropoff 7 16 0    # Tile 7 at 16:00 today
)",
      .args =
          {
              ArgSpec{
                  .name = "tile_index",
                  .type = ArgType::Int,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Tile index",
              },
              ArgSpec{
                  .name = "hour",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Hour (0-23), default: current hour",
              },
              ArgSpec{
                  .name = "minute",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Minute (0-59), default: current minute",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* player = ctx.state.active_player();
        if (!player || !player->mafia_state().active_mission.has_value()) {
          return CommandResult::error("Player has no active mafia mission");
        }
        auto& dropoff = player->mafia_state().active_mission->dropoff;
        dropoff.tile_index = static_cast<int>(ctx.int_arg(0));
        dropoff.date = ctx.state.time().current_date();
        if (ctx.args.size() >= 2) {
          dropoff.hour = static_cast<int>(ctx.int_arg(1));
        }
        else {
          dropoff.hour = ctx.state.time().current_hour();
        }
        if (ctx.args.size() >= 3) {
          dropoff.minute = static_cast<int>(ctx.int_arg(2));
        }
        else {
          dropoff.minute = ctx.state.time().current_minute();
        }
        auto* city = ctx.state.active_city();
        return CommandResult::success().add_message(
            compat::format("Dropoff: {}", location_info(dropoff, city)));
      },
  };
}

static CommandDef make_mafia_set_deadline() {
  return CommandDef{
      .name = "set-deadline",
      .description = "Set the mission deadline to N days from now",
      .help_text = R"(Usage: actor mafia set-deadline <days>

Sets the active mission deadline to N days from the current game date.

Examples:
  actor mafia set-deadline 1   # Due tomorrow
  actor mafia set-deadline 7   # Due in a week
)",
      .args =
          {
              ArgSpec{
                  .name = "days",
                  .type = ArgType::Int,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Days from now",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* player = ctx.state.active_player();
        if (!player || !player->mafia_state().active_mission.has_value()) {
          return CommandResult::error("Player has no active mafia mission");
        }
        int days = static_cast<int>(ctx.int_arg(0));
        auto& m = *player->mafia_state().active_mission;
        m.deadline = ctx.state.time().current_date().add_days(days);
        return CommandResult::success().add_message(compat::format(
            "Deadline set to {} {}:{:02}", m.deadline.to_string(),
            m.deadline_hour, m.deadline_minute));
      },
  };
}

CommandDef make_mafia_commands() {
  return CommandDef{
      .name = "mafia",
      .description = "Mafia interaction commands",
      .help_text = "Trigger mafia events and inspect mafia state",
  }
      .add_subcommand(make_mafia_shakedown())
      .add_subcommand(make_mafia_loan_due())
      .add_subcommand(make_mafia_mission_overdue())
      .add_subcommand(make_mafia_status())
      .add_subcommand(make_mafia_set_bust())
      .add_subcommand(make_mafia_set_pickup())
      .add_subcommand(make_mafia_set_dropoff())
      .add_subcommand(make_mafia_set_deadline());
}

static CommandDef make_police_arrest() {
  return CommandDef{
      .name = "arrest",
      .description = "Trigger a police arrest with a given crime code",
      .help_text = R"(Usage: police arrest <crime_code>

Triggers a police arrest for the given crime code (1-9):
  1=Receiving, 2=Burglary, 3=WeaponsDealing, 4=LaunderingMoney,
  5=ShadyDealings, 6=SuspectedHomicide, 7=PossessingWeapons

Example:
  police arrest 3
)",
      .args =
          {
              ArgSpec{
                  .name = "crime_code",
                  .type = ArgType::Int,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Crime code (1-7)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        int code = static_cast<int>(ctx.int_arg(0));
        if (code < 1 || code > 7) {
          return CommandResult::error("Crime code must be between 1 and 7");
        }
        auto reason = static_cast<ArrestReason>(code);
        ctx.state.initiate_arrest(reason);
        return CommandResult::success().add_message(
            compat::format("Triggered police arrest for crime code {}", code));
      },
  };
}

static CommandDef make_police_visit() {
  return CommandDef{
      .name = "visit",
      .description = "Trigger a voluntary police station visit",
      .help_text = R"(Usage: police visit

Opens the police conversation as a voluntary visit.

Example:
  police visit
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }
        auto police_actors = city->actors(ActorType::POLICE);
        if (police_actors.empty()) {
          return CommandResult::error("No police actor in this city");
        }
        ctx.state.ui().push<PoliceState>(
            PoliceState{.id = police_actors[0]->id(), .reason = std::nullopt});
        return CommandResult::success().add_message(
            "Triggered voluntary police visit");
      },
  };
}

static CommandDef make_police_tip() {
  return CommandDef{
      .name = "tip",
      .description = "Show what the mayor-visit tip would say",
      .help_text = R"(Usage: police tip

Reports the restaurant the mayor tip would name and the countdown it would
quote, or that no visit is near enough to be available.

Example:
  police tip
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* player = ctx.state.active_player();
        if (!player) {
          return CommandResult::error("No active player");
        }
        auto tip = PoliceConversation::mayor_tip(ctx.state, player->id());
        if (!tip) {
          return CommandResult::success().add_message(
              "Mayor tip: nothing available");
        }
        return CommandResult::success().add_message(
            compat::format("Mayor tip: {} in exactly {} days", tip->restaurant,
                           tip->countdown_days));
      },
  };
}

static CommandDef make_police_status() {
  return CommandDef{
      .name = "status",
      .description = "Show player's criminal record and jail status",
      .help_text = R"(Usage: police status

Shows criminal record, jail days left, and social score/rank.

Example:
  police status
)",
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* player = ctx.state.active_player();
        if (!player) {
          return CommandResult::error("No active player");
        }
        auto result = CommandResult::success();
        result.add_message(
            compat::format("Criminal record: {}", player->criminal_record()));
        if (player->is_in_jail()) {
          result.add_message(
              compat::format("Jail days left: {}", *player->jail_days_left()));
        }
        else {
          result.add_message("Jail: free");
        }
        auto& ss = player->social_status();
        int s_rank =
            social_rank(ss.social_score(), ctx.state.difficulty_level());
        int u_rank =
            social_rank(ss.underworld_score(), ctx.state.difficulty_level());
        result.add_message(compat::format("Social: score={} rank={}",
                                          ss.social_score(), s_rank));
        result.add_message(compat::format("Underworld: score={} rank={}",
                                          ss.underworld_score(), u_rank));
        return result;
      },
  };
}

CommandDef make_police_commands() {
  return CommandDef{
      .name = "police",
      .description = "Police interaction commands",
      .help_text = "Trigger police events (arrest, visit, tip, status)",
  }
      .add_subcommand(make_police_arrest())
      .add_subcommand(make_police_visit())
      .add_subcommand(make_police_tip())
      .add_subcommand(make_police_status());
}

CommandDef make_armsdealer_commands() {
  return CommandDef{
      .name = "armsdealer",
      .description = "Arms dealer commands",
      .help_text = "Inspect weapon market prices across all cities",
  }
      .add_subcommand(CommandDef{
          .name = "market",
          .description = "Print weapon buy/sell prices for every city",
          .help_text = R"(Usage: armsdealer market

Prints the purchase and sell price of every weapon for all 10 cities.

Example:
  armsdealer market
)",
          .handler = [](const CommandContext& ctx) -> CommandResult {
            auto result = CommandResult::success();
            auto& market = ctx.state.weapon_market();
            for (int i = 0; i < NUMBER_OF_CITIES; ++i) {
              core::CityId city_id{i};
              result.add_message(compat::format("--- {} (city {}) ---",
                                                core::city_name(city_id), i));
              for (core::item::Item item : core::item::weapon_list) {
                // Only print weapons
                if (static_cast<int>(item) <
                    static_cast<int>(core::item::Item::FlameThrower)) {
                  continue;
                }
                result.add_message(compat::format(
                    "  {:16} buy={:w}  sell={:w}", core::item::to_string(item),
                    market.purchase_price(city_id, item),
                    market.sell_price(city_id, item)));
              }
            }
            return result;
          },
      });
}

} // namespace debug::commands
