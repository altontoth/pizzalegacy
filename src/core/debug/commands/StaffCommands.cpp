/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/commands/StaffCommands.h"

#include "core/character/Character.h"
#include "core/character/Staff.h"
#include "core/character/Training.h"
#include "core/city/Property.h"
#include "core/debug/command/CommonArgs.h"
#include "core/factories/CharacterFactory.h"
#include "core/factories/TrainingFactory.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/StaffEventData.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"

#include <array>
#include <cstddef>

using namespace debug::command;
using core::character::JobRole;
using core::restaurant::StaffEventData;

namespace debug::commands {

// Slot layout: 0-5 = cooks, 6-11 = waiters, 12 = manager.
// Returns a pointer into the restaurant's staff arrays, or nullptr if empty.
// role_out and array_index_out are set on success.
static Staff* resolve_staff(Restaurant* restaurant, int slot, JobRole& role_out,
                            int& array_index_out) {
  if (slot >= 0 && slot <= 5) {
    role_out = JobRole::CHEF;
    array_index_out = slot;
    auto& opt = restaurant->cooks()[slot];
    return opt ? &(*opt) : nullptr;
  }
  if (slot >= 6 && slot <= 11) {
    role_out = JobRole::WAITER;
    array_index_out = slot - 6;
    auto& opt = restaurant->waiters()[slot - 6];
    return opt ? &(*opt) : nullptr;
  }
  if (slot == 12) {
    role_out = JobRole::MANAGER;
    array_index_out = 0;
    auto& opt = restaurant->manager();
    return opt ? &(*opt) : nullptr;
  }
  return nullptr;
}

static std::string_view role_name(JobRole role) {
  switch (role) {
  case JobRole::CHEF:
    return "Cook";
  case JobRole::WAITER:
    return "Waiter";
  case JobRole::MANAGER:
    return "Manager";
  default:
    return "None";
  }
}

// Validate restaurant ownership and return a pointer via out.
// Returns nullopt on success, or an error CommandResult on failure.
static std::optional<CommandResult>
check_restaurant(const CommandContext& ctx, core::PlayerId player_id,
                 core::RestaurantId restaurant_id, Restaurant*& out) {
  out = ctx.state.restaurant(restaurant_id);
  if (!out) {
    return CommandResult::error(
        compat::format("Restaurant {} not found", restaurant_id.value));
  }
  return std::nullopt;
}

static CommandDef make_staff_event() {
  return CommandDef{
      .name = "event",
      .description = "Trigger a staff event dialog directly",
      .help_text =
          R"(Usage: staff event <type> [player_id] [restaurant_id] [slot]

Trigger a staff event dialog for a specific staff member. Useful for testing
the AlertPopup -> OptionsPopup -> reply chain without waiting for daily chance.

type:   late | sick | maternity | quit | raise | theft |
        destroycheesestock | blowupoven | spillwine | messeduporders | forgottoopen
slot:   0-5 = cooks, 6-11 = waiters, 12 = manager (default: 0)

Theft has no dialog -- it posts two status messages directly.

Examples:
  staff event late              # first cook, active restaurant
  staff event quit @ @ 6        # first waiter
  staff event destroycheesestock @ @ 1      # second cook
  staff event late @ @ 2 queue  # third cook, waiting rather than shown

when:   now shows the dialog, queue leaves it waiting like a day-start event
        would (default: now)
)",
      .args =
          {
              ArgSpec{
                  .name = "type",
                  .type = ArgType::String,
                  .optional = false,
                  .description = "Event type",
                  .allowed_values =
                      std::vector<std::string>{
                          "late", "sick", "maternity", "quit", "raise", "theft",
                          "destroycheesestock", "blowupoven", "spillwine",
                          "messeduporders", "forgottoopen"},
              },
              active_player_arg(),
              active_restaurant_arg(),
              ArgSpec{
                  .name = "slot",
                  .type = ArgType::Int,
                  .optional = true,
                  .description =
                      "Staff slot (0-5 cooks, 6-11 waiters, 12 manager)",
                  .min_value = 0,
                  .max_value = 12,
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
        auto player_id = ctx.player_id(1);
        auto restaurant_id = ctx.restaurant_id(2);
        auto slot = ctx.has_arg(3) ? static_cast<int>(ctx.int_arg(3)) : 0;

        Restaurant* restaurant = nullptr;
        if (auto err =
                check_restaurant(ctx, player_id, restaurant_id, restaurant)) {
          return *err;
        }

        JobRole role = JobRole::NONE;
        int array_index = -1;
        Staff* staff = resolve_staff(restaurant, slot, role, array_index);
        if (!staff) {
          return CommandResult::error(
              compat::format("No staff in slot {} of restaurant {}", slot,
                             restaurant_id.value));
        }

        // Theft: no dialog
        if (type_str == "theft") {
          return CommandResult::error({"Can't trigger theft"});
        }

        // Map type string to StaffEventData::Type and set event flags.
        StaffEventData::Type event_type;
        if (type_str == "late") {
          event_type = StaffEventData::Type::Late;
          staff->late(true);
        }
        else if (type_str == "sick") {
          event_type = StaffEventData::Type::Sick;
          staff->sick(true);
          staff->days_away(3);
        }
        else if (type_str == "maternity") {
          event_type = StaffEventData::Type::Maternity;
          staff->pregnant(true);
          staff->days_away(90);
        }
        else if (type_str == "quit") {
          event_type = StaffEventData::Type::Quit;
          staff->wants_to_quit(true);
        }
        else if (type_str == "raise") {
          event_type = StaffEventData::Type::Raise;
          staff->demands_raise(true);
        }
        else if (type_str == "destroycheesestock") {
          event_type = StaffEventData::Type::DestroyCheeseStock;
        }
        else if (type_str == "blowupoven") {
          event_type = StaffEventData::Type::BlowUpOven;
        }
        else if (type_str == "spillwine") {
          event_type = StaffEventData::Type::SpillWine;
        }
        else if (type_str == "messeduporders") {
          event_type = StaffEventData::Type::MessedUpOrders;
        }
        else {
          // forgottoopen
          event_type = StaffEventData::Type::ForgotToOpen;
        }

        StaffEventData event{
            .job_role = role,
            .slot_index = array_index,
            .type = event_type,
        };

        if (ctx.has_arg(4) && ctx.string_arg(4) == "queue") {
          ctx.state.turn().restore_deferred_staff_event(event,
                                                        restaurant->id());
          return CommandResult::success(
              {compat::format("Queued '{}' event for {} {} (slot {})", type_str,
                              staff->character().first_name(),
                              staff->character().last_name(), slot)});
        }

        // show_staff_event_dialog applies the PERSF effect itself.
        ctx.state.show_staff_event_dialog(event, restaurant, []() {});

        return CommandResult::success(
            {compat::format("Triggered '{}' event for {} {} (slot {})",
                            type_str, staff->character().first_name(),
                            staff->character().last_name(), slot)});
      },
  };
}

static CommandDef make_staff_set_motivation() {
  return CommandDef{
      .name = "set_motivation",
      .description = "Set a staff member's motivation directly",
      .help_text =
          R"(Usage: staff set_motivation <value> [player_id] [restaurant_id] [slot]

Set the motivation of a staff member to the given value (0-100).
Useful for getting quit/raise events to trigger easily.

Examples:
  staff set_motivation 0        # zero out first cook's motivation
  staff set_motivation 50 @ @ 6 # set first waiter to 50
)",
      .args =
          {
              ArgSpec{
                  .name = "value",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "Target motivation (0-100)",
                  .min_value = 0,
                  .max_value = 100,
              },
              active_player_arg(),
              active_restaurant_arg(),
              ArgSpec{
                  .name = "slot",
                  .type = ArgType::Int,
                  .optional = true,
                  .description =
                      "Staff slot (0-5 cooks, 6-11 waiters, 12 manager)",
                  .min_value = 0,
                  .max_value = 12,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto value = static_cast<int>(ctx.int_arg(0));
        auto player_id = ctx.player_id(1);
        auto restaurant_id = ctx.restaurant_id(2);
        auto slot = ctx.has_arg(3) ? static_cast<int>(ctx.int_arg(3)) : 0;

        Restaurant* restaurant = nullptr;
        if (auto err =
                check_restaurant(ctx, player_id, restaurant_id, restaurant)) {
          return *err;
        }

        JobRole role = JobRole::NONE;
        int array_index = -1;
        Staff* staff = resolve_staff(restaurant, slot, role, array_index);
        if (!staff) {
          return CommandResult::error(
              compat::format("No staff in slot {} of restaurant {}", slot,
                             restaurant_id.value));
        }

        int current = staff->character().motivation();
        staff->adjust_motivation(value - current);

        return CommandResult::success(
            {compat::format("Set motivation of {} {} from {} to {}",
                            staff->character().first_name(),
                            staff->character().last_name(), current, value)});
      },
  };
}

static CommandDef make_staff_set_days_away() {
  return CommandDef{
      .name = "set_days_away",
      .description = "Set the absence counter for a staff member",
      .help_text =
          R"(Usage: staff set_days_away <days> [player_id] [restaurant_id] [slot]

Set the days-away counter for a staff member. Useful for testing the
"staff comes back" path without waiting.

Examples:
  staff set_days_away 0         # bring first cook back immediately
  staff set_days_away 5 @ @ 6   # put first waiter on 5-day absence
)",
      .args =
          {
              ArgSpec{
                  .name = "days",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "Days away (0 to return immediately)",
                  .min_value = 0,
                  .max_value = 365,
              },
              active_player_arg(),
              active_restaurant_arg(),
              ArgSpec{
                  .name = "slot",
                  .type = ArgType::Int,
                  .optional = true,
                  .description =
                      "Staff slot (0-5 cooks, 6-11 waiters, 12 manager)",
                  .min_value = 0,
                  .max_value = 12,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto days = static_cast<int>(ctx.int_arg(0));
        auto player_id = ctx.player_id(1);
        auto restaurant_id = ctx.restaurant_id(2);
        auto slot = ctx.has_arg(3) ? static_cast<int>(ctx.int_arg(3)) : 0;

        Restaurant* restaurant = nullptr;
        if (auto err =
                check_restaurant(ctx, player_id, restaurant_id, restaurant)) {
          return *err;
        }

        JobRole role = JobRole::NONE;
        int array_index = -1;
        Staff* staff = resolve_staff(restaurant, slot, role, array_index);
        if (!staff) {
          return CommandResult::error(
              compat::format("No staff in slot {} of restaurant {}", slot,
                             restaurant_id.value));
        }

        staff->days_away(days);

        return CommandResult::success({compat::format(
            "Set days_away for {} {} to {}", staff->character().first_name(),
            staff->character().last_name(), days)});
      },
  };
}

static CommandDef make_staff_hire() {
  return CommandDef{
      .name = "hire",
      .description = "Hire character into a restaurant's staff slot",
      .help_text =
          R"(Usage: staff hire <role> [player_id] [restaurant_id] [character_id]

Roles are 0 for a cook, 1 for a waiter and 2 for a manager. The wage is the
character's starting rate for the role, and the job market is left alone, so
the same person is still offered on the screen.

Examples:
  staff hire 2                  # character 0 as a manager
  staff hire 0 @ @ 7            # character 7 as a cook
)",
      .args =
          {
              ArgSpec{
                  .name = "role",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "0 cook, 1 waiter, 2 manager",
                  .min_value = 0,
                  .max_value = 2,
              },
              active_player_arg(),
              active_restaurant_arg(),
              ArgSpec{
                  .name = "character_id",
                  .type = ArgType::Int,
                  .optional = true,
                  .description = "Character to take on (0-199)",
                  .min_value = 0,
                  .max_value = 199,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        constexpr std::array roles = {JobRole::CHEF, JobRole::WAITER,
                                      JobRole::MANAGER};
        JobRole role = roles[static_cast<std::size_t>(ctx.int_arg(0))];
        auto player_id = ctx.player_id(1);
        auto restaurant_id = ctx.restaurant_id(2);
        auto character_id =
            ctx.has_arg(3) ? static_cast<int>(ctx.int_arg(3)) : 0;

        Restaurant* restaurant = nullptr;
        if (auto err =
                check_restaurant(ctx, player_id, restaurant_id, restaurant)) {
          return *err;
        }

        auto* data = CharacterFactory::create_character(character_id, role);
        if (!data) {
          return CommandResult::error(
              compat::format("No character {}", character_id));
        }

        Staff staff(role, core::character::Character(data));
        staff.payment(data->starting_payment(role));

        switch (restaurant->add_staff(staff)) {
        case HiringResult::NoSlotsAvailable:
          return CommandResult::error(
              compat::format("Restaurant {} has no free {} slot",
                             restaurant_id.value, role_name(role)));
        case HiringResult::AlreadyHasManager:
          return CommandResult::error(compat::format(
              "Restaurant {} already has a manager", restaurant_id.value));
        case HiringResult::Success:
          break;
        }

        return CommandResult::success(
            {compat::format("Hired {} {} {} into restaurant {} on ${}",
                            role_name(role), data->first_name, data->last_name,
                            restaurant_id.value, staff.payment().whole())});
      },
  };
}

static CommandDef make_staff_manager() {
  return CommandDef{
      .name = "manager",
      .description = "Set one of the manager settings",
      .help_text =
          R"(Usage: staff manager <setting> <value> [player_id] [restaurant_id]

The settings the manager screen offers. They all start at 0.

shopping, prices, overdraw     0 or 1
turnover_down, turnover_up, supply, staff
                               0 do nothing, 1 call me, 2 act yourself
behaviour                      0 merciless to 6 warmly

Examples:
  staff manager supply 1        # they call about a shortage
  staff manager shopping 1      # they restock without asking
)",
      .args =
          {
              ArgSpec{
                  .name = "setting",
                  .type = ArgType::String,
                  .optional = false,
                  .description = "Which setting",
                  .allowed_values =
                      std::vector<std::string>{"shopping", "prices", "overdraw",
                                               "turnover_down", "turnover_up",
                                               "supply", "staff", "behaviour"},
              },
              ArgSpec{
                  .name = "value",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "The setting's value",
                  .min_value = 0,
                  .max_value = 6,
              },
              active_player_arg(),
              active_restaurant_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto setting = ctx.string_arg(0);
        auto value = static_cast<int>(ctx.int_arg(1));
        auto player_id = ctx.player_id(2);
        auto restaurant_id = ctx.restaurant_id(3);

        Restaurant* restaurant = nullptr;
        if (auto err =
                check_restaurant(ctx, player_id, restaurant_id, restaurant)) {
          return *err;
        }

        auto& settings = restaurant->manager_settings();

        if (setting == "behaviour") {
          settings.behavior_towards_staff = static_cast<ManagerBehavior>(value);
        }
        else if (setting == "shopping" || setting == "prices" ||
                 setting == "overdraw") {
          if (value > 1) {
            return CommandResult::error(
                compat::format("{} is 0 or 1", setting));
          }
          bool on = value != 0;
          if (setting == "shopping") {
            settings.shopping_if_needed = on;
          }
          else if (setting == "prices") {
            settings.change_prices = on;
          }
          else {
            settings.overdraw_account = on;
          }
        }
        else {
          if (value > 2) {
            return CommandResult::error(
                compat::format("{} runs 0 to 2", setting));
          }
          auto reaction = static_cast<ManagerReaction>(value);
          if (setting == "turnover_down") {
            settings.turnover_decrease = reaction;
          }
          else if (setting == "turnover_up") {
            settings.turnover_increase = reaction;
          }
          else if (setting == "supply") {
            settings.supply_shortage = reaction;
          }
          else {
            settings.staff_shortage = reaction;
          }
        }

        return CommandResult::success(
            {compat::format("Set {} to {} for restaurant {}", setting, value,
                            restaurant_id.value)});
      },
  };
}

static CommandDef make_staff_train() {
  return CommandDef{
      .name = "train",
      .description = "Send a staff member on a training course",
      .help_text =
          R"(Usage: staff train <course> [player_id] [restaurant_id] [slot]

Start a course without the skill requirement or the fee. Courses 0-5 are
cooking, 6-11 serving and 12-17 management.

Examples:
  staff train 0                 # first cook starts Cookery Course 1
  staff train 6 @ @ 6           # first waiter starts the serving course
)",
      .args =
          {
              ArgSpec{
                  .name = "course",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "Course index (0-17)",
                  .min_value = 0,
                  .max_value = 17,
              },
              active_player_arg(),
              active_restaurant_arg(),
              ArgSpec{
                  .name = "slot",
                  .type = ArgType::Int,
                  .optional = true,
                  .description =
                      "Staff slot (0-5 cooks, 6-11 waiters, 12 manager)",
                  .min_value = 0,
                  .max_value = 12,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto course = static_cast<int>(ctx.int_arg(0));
        auto player_id = ctx.player_id(1);
        auto restaurant_id = ctx.restaurant_id(2);
        auto slot = ctx.has_arg(3) ? static_cast<int>(ctx.int_arg(3)) : 0;

        Restaurant* restaurant = nullptr;
        if (auto err =
                check_restaurant(ctx, player_id, restaurant_id, restaurant)) {
          return *err;
        }

        JobRole role = JobRole::NONE;
        int array_index = -1;
        Staff* staff = resolve_staff(restaurant, slot, role, array_index);
        if (!staff) {
          return CommandResult::error(
              compat::format("No staff in slot {} of restaurant {}", slot,
                             restaurant_id.value));
        }
        if (staff->on_course()) {
          return CommandResult::error(compat::format(
              "{} {} is already on a course", staff->character().first_name(),
              staff->character().last_name()));
        }

        Training training =
            TrainingFactory::create_training(static_cast<TrainingType>(course));
        staff->send_on_training(training);

        return CommandResult::success({compat::format(
            "Sent {} {} on {} for {} days", staff->character().first_name(),
            staff->character().last_name(), training.name(),
            training.duration())});
      },
  };
}

static CommandDef make_staff_finish_training() {
  return CommandDef{
      .name = "finish_training",
      .description = "Shorten active training courses for fast testing",
      .help_text =
          R"(Usage: staff finish_training [days] [player_id] [restaurant_id]

Set the remaining course days for every staff member currently on a training
course. Defaults to 1, so the course resolves on the next end-of-day tick.

Examples:
  staff finish_training         # all on-course staff finish after one more day
  staff finish_training 3 @ 0   # 3 days left for restaurant 0's trainees
)",
      .args =
          {
              ArgSpec{
                  .name = "days",
                  .type = ArgType::Int,
                  .optional = true,
                  .description = "Days left on the course (>= 1)",
                  .min_value = 1,
                  .max_value = 365,
              },
              active_player_arg(),
              active_restaurant_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        // The Zero provider yields 0; the real default is 1.
        int days = ctx.has_arg(0) ? static_cast<int>(ctx.int_arg(0)) : 1;
        auto player_id = ctx.player_id(1);
        auto restaurant_id = ctx.restaurant_id(2);

        Restaurant* restaurant = nullptr;
        if (auto err =
                check_restaurant(ctx, player_id, restaurant_id, restaurant)) {
          return *err;
        }

        int affected = 0;
        auto shorten = [&](std::optional<Staff>& opt) {
          if (opt && opt->on_course()) {
            opt->days_away(days);
            ++affected;
          }
        };
        for (auto& cook : restaurant->cooks()) {
          shorten(cook);
        }
        for (auto& waiter : restaurant->waiters()) {
          shorten(waiter);
        }
        shorten(restaurant->manager());

        if (affected == 0) {
          return CommandResult::error({compat::format(
              "No staff are on a training course in restaurant {}",
              restaurant_id.value)});
        }

        return CommandResult::success({compat::format(
            "Set {} trainee(s) in restaurant {} to {} day(s) remaining",
            affected, restaurant_id.value, days)});
      },
  };
}

static CommandDef make_staff_list() {
  return CommandDef{
      .name = "list",
      .description = "List all staff in a restaurant",
      .help_text = R"(Usage: staff list [player_id] [restaurant_id]

Print all staff slots with name, role, motivation, days_away,
and current event flags.

Examples:
  staff list            # active player's active restaurant
  staff list @ 0        # restaurant 0 of active player
)",
      .args =
          {
              active_player_arg(),
              active_restaurant_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto restaurant_id = ctx.restaurant_id(1);

        Restaurant* restaurant = nullptr;
        if (auto err =
                check_restaurant(ctx, player_id, restaurant_id, restaurant)) {
          return *err;
        }

        std::vector<std::string> lines;
        lines.push_back(
            compat::format("Staff in restaurant {}:", restaurant_id.value));

        auto print_slot = [&](int slot, const std::optional<Staff>& opt,
                              JobRole role) {
          if (!opt) {
            lines.push_back(
                compat::format("  ({:2d}) {} -- empty", slot, role_name(role)));
            return;
          }
          const Staff& s = *opt;
          std::string flags;
          if (s.is_late()) {
            flags += " late";
          }
          if (s.is_sick()) {
            flags += " sick";
          }
          if (s.is_pregnant()) {
            flags += " pregnant";
          }
          if (s.stole_money()) {
            flags += " theft";
          }
          if (s.wants_to_quit()) {
            flags += " quit";
          }
          if (s.demands_raise()) {
            flags += " raise";
          }
          if (flags.empty()) {
            flags = " -";
          }

          lines.push_back(compat::format(
              "  ({:2d}) {} -- {} {} | motiv: {} away: {} skill: {} ppd: {} "
              "|{}",
              slot, role_name(role), s.character().first_name(),
              s.character().last_name(), s.character().motivation(),
              s.days_away_remaining(),
              role == core::character::JobRole::MANAGER
                  ? s.character().management()
                  : (role == core::character::JobRole::WAITER
                         ? s.character().serving()
                         : s.character().cooking()),
              s.character().pizzas_per_day(), flags));
        };

        auto& cooks = restaurant->cooks();
        for (int i = 0; i < static_cast<int>(cooks.size()); ++i) {
          print_slot(i, cooks[i], JobRole::CHEF);
        }
        auto& waiters = restaurant->waiters();
        for (int i = 0; i < static_cast<int>(waiters.size()); ++i) {
          print_slot(i + 6, waiters[i], JobRole::WAITER);
        }
        print_slot(12, restaurant->manager(), JobRole::MANAGER);

        return CommandResult::success(lines);
      },
  };
}

CommandDef make_staff_commands() {
  return CommandDef{
      .name = "staff",
      .aliases = {"st"},
      .description = "Staff management and event testing commands",
      .help_text = "Subcommands: event, hire, manager, set_motivation, "
                   "set_days_away, train, finish_training, list",
  }
      .add_subcommand(make_staff_event())
      .add_subcommand(make_staff_hire())
      .add_subcommand(make_staff_manager())
      .add_subcommand(make_staff_set_motivation())
      .add_subcommand(make_staff_set_days_away())
      .add_subcommand(make_staff_train())
      .add_subcommand(make_staff_finish_training())
      .add_subcommand(make_staff_list());
}

} // namespace debug::commands
