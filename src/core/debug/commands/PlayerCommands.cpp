/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "PlayerCommands.h"
#include "core/Demographics.h"
#include "core/character/Character.h"
#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/common/Ids.h"
#include "core/debug/command/CommonArgs.h"
#include "core/economy/WeaponMarket.h"
#include "core/factories/PizzaFactory.h"
#include "core/finance/BalanceSheet.h"
#include "core/finance/Ledger.h"
#include "core/finance/Loan.h"
#include "core/finance/Money.h"
#include "core/food/Ingredient.h"
#include "core/food/Pizza.h"
#include "core/food/PlayerRecipes.h"
#include "core/food/Recipe.h"
#include "core/food/RecipeCodec.h"
#include "core/food/StandardPizza.h"
#include "core/food/Topping.h"
#include "core/inventory/Inventory.h"
#include "core/item/Item.h"
#include "core/marketing/AdvertisingCampaign.h"
#include "core/state/GameState.h"
#include "core/time/TimeOfDay.h"
#include "core/util/Constants.h"
#include "core/util/GameContext.h"
#include "engine/FormatCompat.h"
#include "resources/BobSizeFn.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

using namespace debug::command;

namespace debug::commands {

static CommandDef make_player_info() {
  return CommandDef{
      .name = "info",
      .description = "Show player information",
      .help_text = R"(Usage: player info [player_id]

Displays detailed information about a player including name, cash balance,
age, health, energy and the hours their day runs.

Examples:
  player info          # Show active player info
  player info @        # Show active player info (explicit)
  player info 0        # Show player 0 info
  player info 2        # Show player 2 info
)",
      .args = {active_player_arg()},
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto* player = ctx.state.player(player_id);

        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
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

        const auto& character = player->character();
        lines.push_back(compat::format("Age: {}  Health: {}  Energy: {}",
                                       character.age(), character.health(),
                                       character.energy()));

        lines.push_back(compat::format("Day runs {} to {}",
                                       player->start_of_day_time(),
                                       player->end_of_day_time()));

        if (player->is_on_holiday()) {
          lines.push_back(compat::format("Holiday days left: {}",
                                         player->away_days_left()));
        }
        if (player->is_in_jail()) {
          lines.push_back(
              compat::format("Jail days left: {}", player->away_days_left()));
        }

        return CommandResult::success(lines);
      },
  };
}

static CommandDef make_player_active() {
  return CommandDef{
      .name = "active",
      .description = "Make a player the active one",
      .help_text = R"(Usage: player active <player_id>

Switches whose turn the screens and commands act on.

Examples:
  player active 3
)",
      .args =
          {
              ArgSpec{
                  .name = "player_id",
                  .type = ArgType::PlayerId,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Player ID",
                  .min_value = 0,
                  .max_value = NUMBER_OF_PLAYERS - 1,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }
        ctx.state.active_player(player_id);
        return CommandResult::success().add_message(compat::format(
            "Active player is now {} ({})", player_id.value, player->name()));
      },
  };
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
              active_player_arg(),
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
          financials.transfer(today, Account::Capital, Account::Cash, delta,
                              balance_sheet_only);
          return CommandResult::success()
              .add_message(compat::format("Added ${:n} to cash", delta))
              .add_message(
                  compat::format("New balance: ${:n}", player->cash_balance()));
        }
        else {
          Money delta = current - amount;
          financials.transfer(today, Account::Cash, Account::Capital, delta,
                              balance_sheet_only);
          return CommandResult::success()
              .add_message(compat::format("Removed ${:n} from cash", delta))
              .add_message(
                  compat::format("New balance: ${:n}", player->cash_balance()));
        }
      },
  };
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
              active_player_arg(),
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
        player->financials().transfer(today, Account::Capital, Account::Cash,
                                      amount, balance_sheet_only);

        return CommandResult::success()
            .add_message(compat::format("Added ${:n} to cash", amount))
            .add_message(
                compat::format("New balance: ${:n}", player->cash_balance()));
      },
  };
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
              active_player_arg(),
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
        player->financials().transfer(today, Account::Cash, Account::Capital,
                                      amount, balance_sheet_only);

        return CommandResult::success()
            .add_message(compat::format("Removed ${:n} from cash", amount))
            .add_message(
                compat::format("New balance: ${:n}", player->cash_balance()));
      },
  };
}

static CommandDef make_player_money() {
  return CommandDef{
      .name = "money",
      .description = "Manage player's cash balance",
      .help_text = "Subcommands: set, add, sub",
  }
      .add_subcommand(make_player_money_set())
      .add_subcommand(make_player_money_add())
      .add_subcommand(make_player_money_sub());
}

static CommandDef make_player_inventory_buy() {
  return CommandDef{
      .name = "buy",
      .description = "Buy weapons at the active city's price",
      .help_text = R"(Usage: player inventory buy <item_id> <amount> [player_id]

Buys weapons at the arms dealer's price in the active city, the same as
buying them from the dealer: the cash is spent and the goods are booked.
Fails if the player cannot afford them.

Examples:
  player inventory buy 0 10        # Buy 10 of item 0
  player inventory buy 3 5 1       # Buy 5 of item 3 for player 1
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
                  .description = "Quantity to buy (1-255)",
                  .min_value = 1,
                  .max_value = 255,
              },
              active_player_arg(),
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

        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city to buy weapons in");
        }

        auto& inv = player->inventory();
        const Money price =
            ctx.state.weapon_market().purchase_price(city->id(), item_id);
        const Money cost = price * amount;

        if (player->cash_balance() < cost) {
          return CommandResult::error(
              compat::format("{} {} costs ${:n}, player {} has ${:n}", amount,
                             core::item::to_string(item_id), cost,
                             player_id.value, player->cash_balance()));
        }

        const int added = player->buy_weapons(ctx.state.time().current_date(),
                                              item_id, amount, price);
        if (added == 0) {
          return CommandResult::error(
              compat::format("Player {} cannot carry any more {}",
                             player_id.value, core::item::to_string(item_id)));
        }

        const Money paid = price * added;
        return CommandResult::success().add_message(compat::format(
            "Bought {} {} for ${:n} (carrying {})", added,
            core::item::to_string(item_id), paid, inv.count(item_id)));
      },
  };
}

static CommandDef make_player_inventory_clear() {
  return CommandDef{
      .name = "clear",
      .description = "Clear all items from inventory",
      .help_text = R"(Usage: player inventory clear [player_id]

Removes all items from the player's inventory, writing what they cost off to
profit and loss the way spending them on an attack would.

Examples:
  player inventory clear       # Clear active player's inventory
  player inventory clear 2     # Clear player 2's inventory
)",
      .args = {active_player_arg()},
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto* player = ctx.state.player(player_id);

        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        auto& inv = player->inventory();
        int total = 0;
        Money written_off{0};

        for (auto item_id : core::item::weapon_list) {
          int count = inv.count(item_id);
          if (count > 0) {
            written_off =
                written_off + player->weapon_unit_cost(item_id) * count;
            inv.remove(item_id, count);
            total += count;
          }
        }

        if (written_off.cents > 0) {
          player->financials().transfer(
              ctx.state.time().current_date(), Account::Weapons,
              Account::ProfitAndLoss, written_off, balance_sheet_only);
        }

        return CommandResult::success().add_message(
            compat::format("Cleared {} items from inventory, writing off ${:n}",
                           total, written_off));
      },
  };
}

static CommandDef make_player_inventory() {
  return CommandDef{
      .name = "inventory",
      .aliases = {"inv"},
      .description = "Manage player's inventory",
      .help_text = "Subcommands: buy, clear",
  }
      .add_subcommand(make_player_inventory_buy())
      .add_subcommand(make_player_inventory_clear());
}

static CommandDef make_player_recipes_load() {
  return CommandDef{
      .name = "load",
      .description = "Load standard pizza recipes",
      .help_text = R"(Usage: player recipes load [player_id] [index]

Loads standard pizza recipes from disk. If index is provided, loads only
that recipe. Otherwise loads all standard recipes.

Examples:
  player recipes load          # Load all standard recipes for active player
  player recipes load @ 5      # Load only standard recipe 5 for active player
  player recipes load 2        # Load all recipes for player 2
  player recipes load 2 0      # Load recipe 0 for player 2
)",
      .args =
          {
              active_player_arg(),
              ArgSpec{
                  .name = "index",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description =
                      "Optional recipe index (0-" +
                      std::to_string(core::food::STANDARD_PIZZA_COUNT - 1) +
                      ")",
                  .min_value = 0,
                  .max_value = core::food::STANDARD_PIZZA_COUNT - 1,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);

        std::optional<int> only_index;
        if (ctx.has_arg(1)) {
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
                                      : core::food::STANDARD_PIZZA_COUNT;

        auto& zutaten_bob = ctx.context.bob->get("bob/zutaten.bob");
        auto get_bob_size =
            make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob);

        CommandResult result = CommandResult::success();

        for (int i = begin; i < end_exclusive; ++i) {
          const auto cid = static_cast<core::food::StandardPizza>(i);
          auto pizza = PizzaFactory::create_pizza(
              cid, get_bob_size, ctx.state.customer_taste_profiles());

          if (!pizza) {
            result.add_message(compat::format("skip: {} (missing or invalid)",
                                              core::food::filename_for(cid)));
            continue;
          }

          auto rid = recipes.create_standard(cid, std::move(*pizza));
          result.add_message(compat::format(
              "ok: {} -> slot {}", core::food::filename_for(cid), rid.value));
          ++loaded;
        }

        if (only_index.has_value()) {
          result.add_message(
              compat::format("Loaded standard pizza {} (total loaded: {}).",
                             *only_index, loaded));
        }
        else {
          result.add_message(
              compat::format("Loaded {} standard pizzas.", loaded));
        }

        return result;
      },
  };
}

static CommandDef make_player_recipes_import() {
  return CommandDef{
      .name = "import",
      .description = "Import a recipe file as a user recipe",
      .help_text = R"(Usage: player recipes import <file> [player_id]

Imports a .pc recipe file the way the import screen does, then prints the
pizza's stats and its score with each customer group.

Examples:
  player recipes import "recipes/r1.pc"
)",
      .args =
          {
              ArgSpec{
                  .name = "file",
                  .type = ArgType::String,
                  .description = "Path to the recipe file",
              },
              active_player_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        const std::string filename = ctx.string_arg(0);
        auto player_id = ctx.player_id(1);
        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        auto& zutaten_bob = ctx.context.bob->get("bob/zutaten.bob");
        auto get_bob_size =
            make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob);

        auto& recipes = player->recipes();
        const auto outcome =
            PizzaFactory::import_recipe(filename, recipes, get_bob_size,
                                        ctx.state.customer_taste_profiles());
        if (outcome == PizzaFactory::ImportResult::Unreadable) {
          return CommandResult::error("Could not read " + filename);
        }
        if (outcome == PizzaFactory::ImportResult::AlreadyListed) {
          return CommandResult::error("Already in the recipe list");
        }

        const auto all = recipes.all();
        const auto& pizza = all.back()->pizza();
        const auto flavors = pizza.flavors();
        CommandResult result = CommandResult::success();
        result.add_message(compat::format(
            "{} (recipe {}): fat {} sweet {} salty {} hot {}, appearance {} "
            "refinement {} normality {}",
            pizza.name(), all.back()->id().value, flavors.fat, flavors.sweet,
            flavors.salty, flavors.hot, pizza.appearance(), pizza.refinement(),
            pizza.normality()));
        const auto popularity = pizza.popularity();
        for (const auto type : ALL_CONSUMER_TYPES) {
          result.add_message(
              compat::format("  {}: {}", to_string(type), popularity[type]));
        }
        return result;
      },
  };
}

static CommandDef make_player_recipes_export() {
  return CommandDef{
      .name = "export",
      .description = "Export a recipe to a recipe file",
      .help_text =
          R"(Usage: player recipes export <recipe_id> <file> [player_id]

Writes the recipe the way the export screen does.

Examples:
  player recipes export 50 "out/Pizza Roma.pc"
)",
      .args =
          {
              ArgSpec{
                  .name = "recipe_id",
                  .type = ArgType::RecipeId,
                  .description = "Recipe to export",
              },
              ArgSpec{
                  .name = "file",
                  .type = ArgType::String,
                  .description = "Path to write the recipe file to",
              },
              active_player_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        const auto recipe_id = ctx.recipe_id(0);
        const std::filesystem::path path = ctx.string_arg(1);
        auto player_id = ctx.player_id(2);
        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }
        const auto* recipe = player->recipes().find(recipe_id);
        if (!recipe) {
          return CommandResult::error(
              compat::format("Recipe {} not found", recipe_id.value));
        }

        auto& zutaten_bob = ctx.context.bob->get("bob/zutaten.bob");
        auto get_bob_size =
            make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob);
        if (!io::recipe::save_prez(recipe->pizza(), path, get_bob_size,
                                   recipe->display_name())) {
          return CommandResult::error("Could not write " + path.string());
        }
        return CommandResult::success().add_message(compat::format(
            "Exported {} to {}", recipe->display_name(), path.string()));
      },
  };
}

static CommandDef make_player_recipes_show() {
  return CommandDef{
      .name = "show",
      .description = "List a recipe's toppings",
      .help_text = R"(Usage: player recipes show <recipe_id> [player_id]

Prints the recipe's imitation score, then each ingredient on it: how many
pieces, how many of those are whole, chopped and minced, and the grams.

Examples:
  player recipes show 1
)",
      .args =
          {
              ArgSpec{
                  .name = "recipe_id",
                  .type = ArgType::RecipeId,
                  .description = "Recipe to show",
              },
              active_player_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        const auto recipe_id = ctx.recipe_id(0);
        auto player_id = ctx.player_id(1);
        const auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }
        const auto* recipe = player->recipes().find(recipe_id);
        if (!recipe) {
          return CommandResult::error(
              compat::format("Recipe {} not found", recipe_id.value));
        }

        const auto& pizza = recipe->pizza();
        std::map<const core::food::Ingredient*, std::array<int, 3>>
            pieces_by_chop;
        for (const auto& topping : pizza.toppings()) {
          auto& pieces = pieces_by_chop[topping->ingredient()];
          const int chop = topping->chopped_state();
          if (chop >= 0 && chop < static_cast<int>(pieces.size())) {
            ++pieces[chop];
          }
        }

        CommandResult result = CommandResult::success();
        result.add_message(compat::format(
            "{}: imitation {}, {} toppings", recipe->display_name(),
            pizza.imitation(), pizza.toppings().size()));
        for (const auto& [ingredient, grams] : pizza.ingredient_grams()) {
          const auto& chop = pieces_by_chop[ingredient];
          result.add_message(compat::format(
              "  {}: {} pieces ({} whole, {} chopped, {} minced), {} g",
              ingredient->name(), chop[0] + chop[1] + chop[2], chop[0], chop[1],
              chop[2], grams));
        }
        return result;
      },
  };
}

static CommandDef make_player_recipes() {
  return CommandDef{
      .name = "recipes",
      .description = "Manage player's pizza recipes",
      .help_text = "Subcommands: load, import, export, show",
  }
      .add_subcommand(make_player_recipes_load())
      .add_subcommand(make_player_recipes_import())
      .add_subcommand(make_player_recipes_export())
      .add_subcommand(make_player_recipes_show());
}

static CommandDef make_player_weapon_profit() {
  return CommandDef{
      .name = "weapon_profit",
      .description = "Set the player's weapon dealing profit",
      .help_text = R"(Usage: player weapon_profit <amount> [player_id]

The running total from the player's arms deals.

Examples:
  player weapon_profit 3000
)",
      .args =
          {
              ArgSpec{
                  .name = "amount",
                  .type = ArgType::Money,
                  .optional = false,
                  .description = "Dollar amount",
              },
              active_player_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto amount = ctx.money_arg(0);
        auto player_id = ctx.player_id(1);

        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        player->weapon_dealing_profit(amount);

        return CommandResult::success().add_message(
            compat::format("Set player {}'s weapon dealing profit to {}",
                           player_id.value, amount.whole()));
      },
  };
}

static CommandDef make_player_competition() {
  return CommandDef{
      .name = "competition",
      .description = "Set an AI player's competition willingness and wins",
      .help_text = R"(Usage: player competition <willingness> <wins> [player_id]

An AI player turns down a challenge while its willingness counter is above
zero. Losing to the human sets the counter, and it decays by one each turn.
The wins are the player's competition wins in the current city.

Examples:
  player competition 0 2        # will accept a challenge, and has won twice
)",
      .args =
          {
              ArgSpec{
                  .name = "willingness",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "Willingness (0-255)",
                  .min_value = 0,
                  .max_value = 255,
              },
              ArgSpec{
                  .name = "wins",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "Competition wins (0-255)",
                  .min_value = 0,
                  .max_value = 255,
              },
              active_player_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto willingness = static_cast<uint8_t>(ctx.int_arg(0));
        auto wins = static_cast<uint8_t>(ctx.int_arg(1));
        auto player_id = ctx.player_id(2);

        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        auto* city = ctx.state.active_city();
        if (!city) {
          return CommandResult::error("No active city");
        }

        player->set_competition_willingness(willingness);
        city->mayor_office().pizza_competition_wins(player_id.value, wins);

        return CommandResult::success().add_message(compat::format(
            "Set player {}'s competition willingness to {} and wins to {}",
            player_id.value, willingness, wins));
      },
  };
}

static CommandDef make_player_ad_effectiveness() {
  return CommandDef{
      .name = "ad_effectiveness",
      .description = "Set one advertising channel's effectiveness in a city",
      .help_text =
          R"(Usage: player ad_effectiveness <channel> <value> [city_id] [player_id]

Effectiveness feeds a restaurant's popularity and keeps decaying after the
campaign that bought it has ended.

Examples:
  player ad_effectiveness 0 60  # channel 0 at 60 in the active city
)",
      .args =
          {
              ArgSpec{
                  .name = "channel",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "Advertising channel (0-3)",
                  .min_value = 0,
                  .max_value = 3,
              },
              ArgSpec{
                  .name = "value",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "Effectiveness (0-255)",
                  .min_value = 0,
                  .max_value = 255,
              },
              active_city_arg(),
              active_player_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto channel = static_cast<std::size_t>(ctx.int_arg(0));
        auto value = static_cast<uint8_t>(ctx.int_arg(1));
        auto city_id = ctx.city_id(2);
        auto player_id = ctx.player_id(3);

        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        std::array<uint8_t, 4> channels{};
        const auto& all = player->ad_effectiveness();
        if (auto it = all.find(city_id); it != all.end()) {
          channels = it->second;
        }
        channels[channel] = value;
        player->ad_effectiveness(city_id, channels);

        return CommandResult::success().add_message(compat::format(
            "Set player {}'s channel {} effectiveness to {} in city {}",
            player_id.value, channel, value, city_id.value));
      },
  };
}

static const char* channel_name(AdvertisingChannel channel) {
  switch (channel) {
  case AdvertisingChannel::FLYERS:
    return "flyers";
  case AdvertisingChannel::POSTERS:
    return "poster";
  case AdvertisingChannel::NEWSPAPER_ADS:
    return "newspaper";
  case AdvertisingChannel::TV_ADS:
    return "tv";
  }
  return "?";
}

static CommandDef make_player_campaigns() {
  return CommandDef{
      .name = "campaigns",
      .description = "List a player's running advertising campaigns",
      .help_text = R"(Usage: player campaigns [player_id]

Prints every city the player has a campaign in, one line per channel, with the
days left, the effectiveness it bought and what was booked. A newspaper
campaign also prints the slogan its five fragments spell out.

Examples:
  player campaigns
  player campaigns 0
)",
      .args = {active_player_arg()},
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        CommandResult result = CommandResult::success();
        int shown = 0;
        for (const auto& [city_id, channels] : player->get_campaigns()) {
          for (const auto& slot : channels) {
            if (!slot) {
              continue;
            }
            ++shown;
            result.add_message(compat::format(
                "city {} {:9} {:3} day(s) left, effectiveness {}, cost {:i}",
                city_id.value, channel_name(slot->channel),
                slot->days_remaining, slot->rate.effectiveness,
                slot->rate.cost));
            if (slot->flyer_count) {
              result.add_message(
                  compat::format("  {} flyers", *slot->flyer_count));
            }
            if (slot->poster_id) {
              result.add_message(
                  compat::format("  poster {}", *slot->poster_id));
            }
            if (slot->tv_ad_id) {
              result.add_message(compat::format("  ad {}", *slot->tv_ad_id));
            }
            if (slot->newspaper_ad_format) {
              result.add_message(compat::format(
                  "  format {}", static_cast<int>(*slot->newspaper_ad_format)));
            }
            if (!slot->newspaper_lines.empty()) {
              std::string slogan;
              for (const auto& part : slot->newspaper_lines) {
                slogan += part;
              }
              result.add_message(compat::format("  \"{}\"", slogan));
            }
          }
        }
        if (shown == 0) {
          result.add_message(compat::format("Player {} is running no campaigns",
                                            player_id.value));
        }
        return result;
      },
  };
}

static CommandDef make_player_balance() {
  return CommandDef{
      .name = "balance",
      .description = "Print a player's balance sheet",
      .help_text = R"(Usage: player balance [player_id]

The same two columns the global stats Balance page shows: assets on the left,
capital, loans and the year's result on the right. The two totals must agree.

Examples:
  player balance
  player balance 0
)",
      .args = {active_player_arg()},
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        BalanceSheet sheet{*player};
        CommandResult result = CommandResult::success();
        result.add_message("Assets:");
        for (const auto& entry : sheet.assets()) {
          result.add_message(compat::format(
              "  {:14} {:i}", to_string(entry.account), entry.amount));
        }
        result.add_message(
            compat::format("  {:14} {:i}", "TOTAL", sheet.total_assets()));
        result.add_message("Liabilities:");
        for (const auto& entry : sheet.liabilities()) {
          result.add_message(compat::format(
              "  {:14} {:i}", to_string(entry.account), entry.amount));
        }
        result.add_message(
            compat::format("  {:14} {:i}", "TOTAL", sheet.total_liabilities()));
        return result;
      },
  };
}

static CommandDef make_player_loan() {
  return CommandDef{
      .name = "loan",
      .description = "Take, pay or repay a bank loan",
      .help_text = R"(Usage: player loan <take|month|repay> [amount]

Works on the active player's loans in the active city.
  take <amount>  books a 12 month loan at 10% from bank 0
  month          books one monthly payment on every open loan
  repay          pays off every open loan at once, interest included

Examples:
  player loan take 5000
  player loan month
  player loan repay
)",
      .args =
          {
              ArgSpec{
                  .name = "action",
                  .type = ArgType::String,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "take, month or repay",
                  .allowed_values =
                      std::vector<std::string>{"take", "month", "repay"},
              },
              ArgSpec::optional_with_default("amount", ArgType::Money,
                                             DefaultProvider::None,
                                             "Loan amount for take"),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto* player = ctx.state.active_player();
        if (!player) {
          return CommandResult::error("No active player");
        }
        const auto date = ctx.state.time().current_date();
        const auto action = ctx.string_arg(0);
        if (action == "take") {
          if (!ctx.has_arg(1)) {
            return CommandResult::error("take needs an amount");
          }
          constexpr uint8_t interest_percentage = 10;
          constexpr uint8_t months = 12;
          player->add_loan(ctx.state.active_city_id(),
                           Loan{core::BankId{0}, ctx.money_arg(1),
                                interest_percentage, months},
                           date);
        }
        else if (action == "month") {
          player->apply_monthly_loan_payments(
              date, ctx.state.rules().fix_legacy_bugs);
        }
        else {
          for (auto& loan : player->loans_in_city(ctx.state.active_city_id())) {
            if (!loan.is_settled()) {
              player->repay_loan(loan, date, ctx.state.rules().fix_legacy_bugs);
            }
          }
        }
        return CommandResult::success().add_message(
            compat::format("Loan account {:i}, repayment this month {:i}",
                           player->financials().balance(Account::Loan),
                           player->financials().current_month(
                               MonthlyCategory::Repayment, date)));
      },
  };
}

static CommandDef make_player_holiday() {
  return CommandDef{
      .name = "holiday",
      .aliases = {"vacation"},
      .description = "Send a player on holiday",
      .help_text = R"(Usage: player holiday <days> [player_id]

Sends the player on holiday for the given number of days. Their turns run
non-interactively until the counter runs down, which it does once per turn
card; there is no command to end one early.

Examples:
  player holiday 7        # Active player leaves for a week
  player holiday 3 1      # Player 1 leaves for three days
)",
      .args =
          {
              ArgSpec{
                  .name = "days",
                  .type = ArgType::Int,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Days of holiday",
                  .min_value = 1,
                  .max_value = 99,
              },
              active_player_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto days = static_cast<int>(ctx.int_arg(0));
        auto player_id = ctx.player_id(1);

        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        player->holiday(days);

        return CommandResult::success().add_message(compat::format(
            "{} is on holiday for {} days", player->name(), days));
      },
  };
}

static CommandDef make_player_startofday() {
  return CommandDef{
      .name = "startofday",
      .description = "Set the hour a player's next day starts",
      .help_text = R"(Usage: player startofday <hour> [player_id]

Sets the hour the player's next day starts, in whole hours. The setting
survives the day it is made on, so a start of day moved earlier just before
leaving on holiday takes effect on the first day away, and it returns to 9
once used.

Examples:
  player startofday 0     # Active player rises at midnight tomorrow
  player startofday 6 1   # Player 1 rises at 6am tomorrow
)",
      .args =
          {
              ArgSpec{
                  .name = "hour",
                  .type = ArgType::Int,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Hour of the day to start at",
                  .min_value = 0,
                  .max_value = Player::default_start_of_day_hour,
              },
              active_player_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto hour = static_cast<int>(ctx.int_arg(0));
        auto player_id = ctx.player_id(1);

        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        player->start_of_day_hour(static_cast<uint8_t>(hour));

        return CommandResult::success().add_message(
            compat::format("{} starts the next day at {}", player->name(),
                           player->start_of_day_time()));
      },
  };
}

static CommandDef make_player_jail() {
  return CommandDef{
      .name = "jail",
      .aliases = {"prison"},
      .description = "Send a player to prison",
      .help_text = R"(Usage: player jail <days> [player_id]

Imprisons the player for the given number of days, the same state an arrest
produces. Their turns run non-interactively until the sentence is served.

Examples:
  player jail 5         # Active player serves five days
  player jail 10 2      # Player 2 serves ten days
)",
      .args =
          {
              ArgSpec{
                  .name = "days",
                  .type = ArgType::Int,
                  .optional = false,
                  .default_provider = DefaultProvider::None,
                  .description = "Days of the sentence",
                  .min_value = 1,
                  .max_value = 99,
              },
              active_player_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto days = static_cast<int>(ctx.int_arg(0));
        auto player_id = ctx.player_id(1);

        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} not found", player_id.value));
        }

        player->jail(days);

        return CommandResult::success().add_message(
            compat::format("{} is in prison for {} days", player->name(),
                           player->away_days_left()));
      },
  };
}

CommandDef make_player_commands() {
  return CommandDef{
      .name = "player",
      .aliases = {"p"},
      .description = "Player management commands",
      .help_text =
          "Manage player state: info, active, money, inventory, "
          "recipes, weapon_profit, competition, campaigns, balance, loan, "
          "ad_effectiveness, holiday, startofday, jail",
  }
      .add_subcommand(make_player_info())
      .add_subcommand(make_player_active())
      .add_subcommand(make_player_money())
      .add_subcommand(make_player_inventory())
      .add_subcommand(make_player_recipes())
      .add_subcommand(make_player_weapon_profit())
      .add_subcommand(make_player_competition())
      .add_subcommand(make_player_campaigns())
      .add_subcommand(make_player_balance())
      .add_subcommand(make_player_loan())
      .add_subcommand(make_player_ad_effectiveness())
      .add_subcommand(make_player_holiday())
      .add_subcommand(make_player_startofday())
      .add_subcommand(make_player_jail());
}

} // namespace debug::commands
