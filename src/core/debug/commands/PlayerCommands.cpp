/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "PlayerCommands.h"
#include "core/character/Player.h"
#include "core/finance/Money.h"
#include "core/food/Pizza.h"
#include "core/state/GameState.h"
#include "core/util/GameContext.h"
#include "factories/PizzaFactory.h"
#include "resources/BobSizeFn.h"

using namespace debug::command;

namespace debug::commands {

// Common argument spec for player_id
static ArgSpec player_id_arg() {
  return ArgSpec{
      .name = "player_id",
      .type = ArgType::PlayerId,
      .optional = true,
      .default_provider = DefaultProvider::ActivePlayer,
      .description = "Player ID (@ for active player)",
      .min_value = 0,
      .max_value = 7,
  };
}

static CommandDef make_player_info() {
  return CommandDef{.name = "info",
                    .description = "Show player information",
                    .help_text = R"(Usage: player info [player_id]

Displays detailed information about a player including name, cash balance,
and other stats.

Examples:
  player info          # Show active player info
  player info @        # Show active player info (explicit)
  player info 0        # Show player 0 info
  player info 2        # Show player 2 info
)",
                    .args = {player_id_arg()},
                    .handler = [](const CommandContext& ctx) -> CommandResult {
                      auto player_id = ctx.player_id(0);
                      auto* player = ctx.state.player(player_id);

                      if (!player) {
                        return CommandResult::error(compat::format(
                            "Player {} not found", player_id.value));
                      }

                      std::ostringstream ss;
                      ss << *player;

                      // Convert to lines
                      std::vector<std::string> lines;
                      std::string line;
                      std::istringstream stream(ss.str());
                      while (std::getline(stream, line)) {
                        lines.push_back(line);
                      }

                      return CommandResult::success(lines);
                    },
                    .category = "player"};
}

static CommandDef make_player_money_set() {
  return CommandDef{
      .name = "set",
      .description = "Set player's cash to specific amount",
      .help_text = R"(Usage: player money set <amount> [player_id]

Sets the cash balance to the specified dollar amount.

Examples:
  player money set 1000        # Set active player to $1000
  player money set 5000 2      # Set player 2 to $5000
  player money set 0           # Reset active player's cash
)",
      .args =
          {
              ArgSpec{
                  .name = "amount",
                  .type = ArgType::Money,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Dollar amount to set",
                  .min_value = 0,
              },
              player_id_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto amount = ctx.money_arg(0);
        auto player_id = ctx.player_id(1);

        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        Money current = player->cash_balance();
        GameDate today = ctx.state.time().current_date();

        if (current == amount) {
          return CommandResult::success().add_message(
              compat::format("No change (already ${:n})", current));
        }

        auto& financials = player->financials();
        if (current < amount) {
          Money delta = amount - current;
          financials.transfer(today, Account::Cheat, Account::Cash, delta,
                              AccountingScope::Capital);
          return CommandResult::success()
              .add_message(compat::format("Added ${:n} to cash", delta))
              .add_message(
                  compat::format("New balance: ${:n}", player->cash_balance()));
        }
        else {
          Money delta = current - amount;
          financials.transfer(today, Account::Cash, Account::Cheat, delta,
                              AccountingScope::Capital);
          return CommandResult::success()
              .add_message(compat::format("Removed ${:n} from cash", delta))
              .add_message(
                  compat::format("New balance: ${:n}", player->cash_balance()));
        }
      },
      .category = "player"};
}

static CommandDef make_player_money_add() {
  return CommandDef{
      .name = "add",
      .description = "Add money to player's cash",
      .help_text = R"(Usage: player money add <amount> [player_id]

Adds the specified dollar amount to the player's cash balance.

Examples:
  player money add 500         # Add $500 to active player
  player money add 1000 1      # Add $1000 to player 1
)",
      .args =
          {
              ArgSpec{
                  .name = "amount",
                  .type = ArgType::Money,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Dollar amount to add",
                  .min_value = 0,
              },
              player_id_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto amount = ctx.money_arg(0);
        auto player_id = ctx.player_id(1);

        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        GameDate today = ctx.state.time().current_date();
        player->financials().transfer(today, Account::Cheat, Account::Cash,
                                      amount, AccountingScope::Capital);

        return CommandResult::success()
            .add_message(compat::format("Added ${:n} to cash", amount))
            .add_message(
                compat::format("New balance: ${:n}", player->cash_balance()));
      },
      .category = "player"};
}

static CommandDef make_player_money_sub() {
  return CommandDef{
      .name = "sub",
      .aliases = {"subtract", "remove"},
      .description = "Subtract money from player's cash",
      .help_text = R"(Usage: player money sub <amount> [player_id]

Subtracts the specified dollar amount from the player's cash balance.

Examples:
  player money sub 200         # Remove $200 from active player
  player money sub 500 3       # Remove $500 from player 3
)",
      .args =
          {
              ArgSpec{
                  .name = "amount",
                  .type = ArgType::Money,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Dollar amount to subtract",
                  .min_value = 0,
              },
              player_id_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto amount = ctx.money_arg(0);
        auto player_id = ctx.player_id(1);

        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        GameDate today = ctx.state.time().current_date();
        player->financials().transfer(today, Account::Cash, Account::Cheat,
                                      amount, AccountingScope::Capital);

        return CommandResult::success()
            .add_message(compat::format("Removed ${:n} from cash", amount))
            .add_message(
                compat::format("New balance: ${:n}", player->cash_balance()));
      },
      .category = "player"};
}

static CommandDef make_player_money() {
  return CommandDef{
      .name = "money",
      .description = "Manage player's cash balance",
      .help_text = "Subcommands: set, add, sub",
      .category = "player",
  }
      .add_subcommand(make_player_money_set())
      .add_subcommand(make_player_money_add())
      .add_subcommand(make_player_money_sub());
}

static CommandDef make_player_inventory_set() {
  return CommandDef{
      .name = "set",
      .description = "Set item quantity in inventory",
      .help_text = R"(Usage: player inventory set <item_id> <amount> [player_id]

Sets the quantity of an item in the player's inventory.

Examples:
  player inventory set 0 10        # Set item 0 to quantity 10
  player inventory set 3 5 1       # Set item 3 to quantity 5 for player 1
)",
      .args =
          {
              ArgSpec{
                  .name = "item_id",
                  .type = ArgType::ItemId,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Item ID",
              },
              ArgSpec{
                  .name = "amount",
                  .type = ArgType::Int,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Quantity (0-255)",
                  .min_value = 0,
                  .max_value = 255,
              },
              player_id_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto item_id = ctx.item_id(0);
        auto amount = static_cast<int>(ctx.int_arg(1));
        auto player_id = ctx.player_id(2);

        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        auto& inv = player->inventory();
        int current = inv.count(item_id);
        int delta = amount - current;

        if (delta > 0) {
          int added = inv.add(item_id, delta);
          return CommandResult::success().add_message(compat::format(
              "Added {} units of {} (total: {})", added,
              core::item::to_string(item_id), inv.count(item_id)));
        }
        else if (delta < 0) {
          int removed = inv.remove(item_id, -delta);
          return CommandResult::success().add_message(compat::format(
              "Removed {} units of {} (total: {})", removed,
              core::item::to_string(item_id), inv.count(item_id)));
        }
        else {
          return CommandResult::success().add_message(
              compat::format("No change (already {})", amount));
        }
      },
      .category = "player"};
}

static CommandDef make_player_inventory_clear() {
  return CommandDef{
      .name = "clear",
      .description = "Clear all items from inventory",
      .help_text = R"(Usage: player inventory clear [player_id]

Removes all items from the player's inventory.

Examples:
  player inventory clear       # Clear active player's inventory
  player inventory clear 2     # Clear player 2's inventory
)",
      .args = {player_id_arg()},
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto* player = ctx.state.player(player_id);

        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        auto& inv = player->inventory();
        int total = 0;

        // Clear all items
        for (size_t i = 0; i < static_cast<size_t>(core::item::Item::Count);
             ++i) {
          auto item_id = static_cast<core::item::Item>(i);
          int count = inv.count(item_id);
          if (count > 0) {
            inv.remove(item_id, count);
            total += count;
          }
        }

        return CommandResult::success().add_message(
            compat::format("Cleared {} items from inventory", total));
      },
      .category = "player"};
}

static CommandDef make_player_inventory() {
  return CommandDef{
      .name = "inventory",
      .aliases = {"inv"},
      .description = "Manage player's inventory",
      .help_text = "Subcommands: set, clear",
      .category = "player",
  }
      .add_subcommand(make_player_inventory_set())
      .add_subcommand(make_player_inventory_clear());
}

static CommandDef make_player_recipes_load() {
  return CommandDef{
      .name = "load",
      .description = "Load canonical pizza recipes",
      .help_text = R"(Usage: player recipes load [player_id] [index]

Loads canonical pizza recipes from disk. If index is provided, loads only
that recipe. Otherwise loads all canonical recipes.

Examples:
  player recipes load          # Load all canonical recipes for active player
  player recipes load @        # Load all canonical recipes for active player (explicit)
  player recipes load @ 5      # Load only canonical recipe 5 for active player
  player recipes load 2        # Load all recipes for player 2
  player recipes load 2 0      # Load recipe 0 for player 2
)",
      .args =
          {
              player_id_arg(),
              ArgSpec{
                  .name = "index",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Optional recipe index (0-" +
                                 std::to_string(core::food::CANON_COUNT - 1) +
                                 ")",
                  .min_value = 0,
                  .max_value = core::food::CANON_COUNT - 1,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);

        std::optional<int> only_index;
        if (ctx.args.size() > 1) {
          only_index = static_cast<int>(ctx.int_arg(1));
        }

        auto* player = ctx.state.player(player_id);

        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        auto& recipes = player->recipes();
        int loaded = 0;

        const int begin = only_index.has_value() ? *only_index : 0;
        const int end_exclusive = only_index.has_value()
                                      ? (*only_index + 1)
                                      : core::food::CANON_COUNT;

        auto& zutaten_bob = ctx.context.bob->get("bob/zutaten.bob");
        auto get_bob_size =
            make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob);

        CommandResult result = CommandResult::success();

        for (int i = begin; i < end_exclusive; ++i) {
          const auto cid = static_cast<core::food::CanonicalPizza>(i);
          auto pizza = PizzaFactory::create_pizza(
              i, get_bob_size, ctx.state.customer_taste_profiles());

          if (!pizza) {
            result.add_message(compat::format("skip: {} (missing or invalid)",
                                              core::food::filename_for(cid)));
            continue;
          }

          auto rid = recipes.create_from_canonical(cid, std::move(*pizza));
          result.add_message(compat::format(
              "ok: {} -> slot {}", core::food::filename_for(cid), rid.value));
          ++loaded;
        }

        if (only_index.has_value()) {
          result.add_message(
              compat::format("Loaded canonical pizza {} (total loaded: {}).",
                             *only_index, loaded));
        }
        else {
          result.add_message(
              compat::format("Loaded {} canonical pizzas.", loaded));
        }

        return result;
      },
      .category = "player"};
}

static CommandDef make_player_recipes() {
  return CommandDef{
      .name = "recipes",
      .description = "Manage player's pizza recipes",
      .help_text = "Subcommands: load",
      .category = "player",
  }
      .add_subcommand(make_player_recipes_load());
}

CommandDef make_player_commands() {
  return CommandDef{
      .name = "player",
      .aliases = {"p"},
      .description = "Player management commands",
      .help_text = "Manage player state: info, money, inventory, recipes",
      .category = "player",
  }
      .add_subcommand(make_player_info())
      .add_subcommand(make_player_money())
      .add_subcommand(make_player_inventory())
      .add_subcommand(make_player_recipes());
}

} // namespace debug::commands
