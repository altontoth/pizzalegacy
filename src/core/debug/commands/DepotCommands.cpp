/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/commands/DepotCommands.h"
#include "core/city/City.h"
#include "core/city/Property.h"
#include "core/inventory/Inventory.h"
#include "core/item/Item.h"
#include "core/state/GameState.h"
#include "core/util/Constants.h"
#include "engine/FormatCompat.h"
#include "engine/Random.h"

#include <algorithm>

using namespace debug::command;

namespace debug::commands {

// Helper: find first available depot property in city
static Property* find_available_depot(City& city) {
  for (auto* property : city.properties()) {
    if (property->type() == PropertyType::DEPOT && !property->is_owned() &&
        !property->is_rented()) {
      return property;
    }
  }
  return nullptr;
}

// Depots are numbered in the order the city lists its properties.
static Property* find_player_depot(City& city, core::PlayerId player_id,
                                   int index) {
  int seen = 0;
  for (auto* property : city.properties()) {
    if (property->type() == PropertyType::DEPOT) {
      auto owner = property->owner_or_tenant();
      if (owner && *owner == player_id) {
        if (seen == index) {
          return property;
        }
        ++seen;
      }
    }
  }
  return nullptr;
}

// Helper: count player's depot properties in city
static int count_player_depots(const City& city, core::PlayerId player_id) {
  int count = 0;
  for (auto* property : city.properties()) {
    if (property->type() == PropertyType::DEPOT) {
      auto owner = property->owner_or_tenant();
      if (owner && *owner == player_id) {
        ++count;
      }
    }
  }
  return count;
}

// Common argument specs: (city_id, player_id)
static std::vector<ArgSpec> city_player_args() {
  return {
      ArgSpec{
          .name = "city_id",
          .type = ArgType::CityId,
          .optional = true,
          .default_provider = DefaultProvider::ActiveCity,
          .description = "City ID (@ for active city)",
          .min_value = 0,
          .max_value = 9,
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
  };
}

static CommandDef make_depot_give() {
  return CommandDef{
      .name = "give",
      .description = "Give player a free depot in the city",
      .help_text = R"(Usage: depot give [city_id] [player_id]

Finds an available depot property and rents it for free.
Useful for development/testing.

Examples:
  depot give              # Active city/player
  depot give @ @          # Active city/player (explicit)
  depot give 6 0          # Madrid, player 0
)",
      .args = city_player_args(),
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto city_id = ctx.city_id(0);
        auto player_id = ctx.player_id(1);

        ctx.state.ensure_city_loaded(city_id);
        auto* city = ctx.state.city(city_id);
        if (!city) {
          return CommandResult::error(
              compat::format("Failed to load city {}", city_id.value));
        }

        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} does not exist", player_id.value));
        }

        auto* property = find_available_depot(*city);
        if (!property) {
          return CommandResult::error(
              "No available depot properties in this city");
        }

        bool success = ctx.state.real_estate().rent_depot(*property, *player,
                                                          /*free=*/true);
        if (!success) {
          return CommandResult::error("Failed to rent depot property");
        }

        int tile_x = City::index_to_x(property->index());
        int tile_y = City::index_to_y(property->index());

        return CommandResult::success().add_message(
            compat::format("Gave player {} a depot at tile ({}, {}) in {}",
                           player_id.value, tile_x, tile_y, city->name()));
      },
  };
}

static CommandDef make_depot_info() {
  return CommandDef{
      .name = "info",
      .description = "Show depot information",
      .help_text = R"(Usage: depot info [city_id] [player_id]

Displays depot capacity, usage, and stock summary for a player
in a city.

Examples:
  depot info              # Active city/player
  depot info @ 0          # Active city, player 0
  depot info 6 @          # Madrid, active player
)",
      .args = city_player_args(),
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto city_id = ctx.city_id(0);
        auto player_id = ctx.player_id(1);

        ctx.state.ensure_city_loaded(city_id);
        auto* city = ctx.state.city(city_id);
        if (!city) {
          return CommandResult::error(
              compat::format("Failed to load city {}", city_id.value));
        }

        auto& depot = city->central_depot(player_id);
        int depot_count = count_player_depots(*city, player_id);

        std::vector<std::string> lines;
        lines.push_back(compat::format(
            "Depot info for player {} in {}:", player_id.value, city->name()));
        lines.push_back(
            compat::format("  Depot properties owned/rented: {}", depot_count));
        lines.push_back(
            compat::format("  Capacity: {} units", depot.capacity()));
        lines.push_back(compat::format("  Used: {} units", depot.used()));

        // Count non-zero stocks
        int stocked_ingredients = 0;
        int total_quantity = 0;
        for (int i = 0; i < 90; ++i) {
          auto* stock = depot.get_ingredient_stock(i);
          if (stock && stock->quantity > 0) {
            ++stocked_ingredients;
            total_quantity += stock->quantity;
          }
        }
        lines.push_back(
            compat::format("  Stocked ingredients: {}/90 ({} grams total)",
                           stocked_ingredients, total_quantity));

        return CommandResult::success(lines);
      },
  };
}

static CommandDef make_depot_stock_add() {
  return CommandDef{
      .name = "add",
      .description = "Add stock for a specific ingredient",
      .help_text =
          R"(Usage: depot stock add <ingredient_id> [quantity] [quality] [city_id] [player_id]

Adds stock for a specific ingredient type.

Arguments:
  ingredient_id - Ingredient ID (0-89)
  quantity      - Quantity in grams (default: 10000)
  quality       - Quality 0-100 (default: 100)
  city_id       - City ID (@ for active city)
  player_id     - Player ID (@ for active player)

Examples:
  depot stock add 0              # Add 10kg of ingredient 0 at quality 100
  depot stock add 5 5000         # Add 5kg of ingredient 5 at quality 100
  depot stock add 10 20000 80    # Add 20kg of ingredient 10 at quality 80
)",
      .args =
          {
              ArgSpec{
                  .name = "ingredient_id",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "Ingredient ID (0-89)",
                  .min_value = 0,
                  .max_value = 89,
              },
              ArgSpec{
                  .name = "quantity",
                  .type = ArgType::Int,
                  .optional = true,
                  .description = "Quantity in grams",
                  .min_value = 0,
                  .max_value = 1000000,
              },
              ArgSpec{
                  .name = "quality",
                  .type = ArgType::Int,
                  .optional = true,
                  .description = "Quality 0-100",
                  .min_value = 0,
                  .max_value = 100,
              },
              ArgSpec{
                  .name = "city_id",
                  .type = ArgType::CityId,
                  .optional = true,
                  .default_provider = DefaultProvider::ActiveCity,
                  .description = "City ID (@ for active city)",
                  .min_value = 0,
                  .max_value = 9,
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
        auto ingredient_id = ctx.int_arg(0);
        auto quantity = ctx.args.size() > 1 ? ctx.int_arg(1) : 10000;
        auto quality = ctx.args.size() > 2 ? ctx.int_arg(2) : 100;
        auto city_id = ctx.city_id(3);
        auto player_id = ctx.player_id(4);

        ctx.state.ensure_city_loaded(city_id);
        auto* city = ctx.state.city(city_id);
        if (!city) {
          return CommandResult::error(
              compat::format("Failed to load city {}", city_id.value));
        }

        auto& depot = city->central_depot(player_id);
        auto ingredient_type =
            static_cast<core::food::IngredientType>(ingredient_id);
        depot.add_ingredient(ingredient_type, quantity, quality, Money{100});

        return CommandResult::success().add_message(compat::format(
            "Added {}g of ingredient {} at quality {} to {}'s depot in {}",
            quantity, ingredient_id, quality, player_id.value, city->name()));
      },
  };
}

static CommandDef make_depot_stock_fill() {
  return CommandDef{
      .name = "fill",
      .description = "Fill all ingredient stocks to specified quantity",
      .help_text =
          R"(Usage: depot stock fill [quantity] [quality] [city_id] [player_id]

Fills all ingredient stocks to the specified quantity and quality.

Arguments:
  quantity      - Quantity in grams per ingredient (default: 10000)
  quality       - Quality 0-100 (default: 100)
  city_id       - City ID (@ for active city)
  player_id     - Player ID (@ for active player)

Examples:
  depot stock fill              # Fill all to 10kg at quality 100
  depot stock fill 20000        # Fill all to 20kg at quality 100
  depot stock fill 15000 80     # Fill all to 15kg at quality 80
)",
      .args =
          {
              ArgSpec{
                  .name = "quantity",
                  .type = ArgType::Int,
                  .optional = true,
                  .description = "Quantity in grams",
                  .min_value = 0,
                  .max_value = 1000000,
              },
              ArgSpec{
                  .name = "quality",
                  .type = ArgType::Int,
                  .optional = true,
                  .description = "Quality 0-100",
                  .min_value = 0,
                  .max_value = 100,
              },
              ArgSpec{
                  .name = "city_id",
                  .type = ArgType::CityId,
                  .optional = true,
                  .default_provider = DefaultProvider::ActiveCity,
                  .description = "City ID (@ for active city)",
                  .min_value = 0,
                  .max_value = 9,
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
        auto quantity = !ctx.args.empty() ? ctx.int_arg(0) : 10000;
        auto quality = ctx.args.size() > 1 ? ctx.int_arg(1) : 100;
        auto city_id = ctx.city_id(2);
        auto player_id = ctx.player_id(3);

        ctx.state.ensure_city_loaded(city_id);
        auto* city = ctx.state.city(city_id);
        if (!city) {
          return CommandResult::error(
              compat::format("Failed to load city {}", city_id.value));
        }

        auto& depot = city->central_depot(player_id);

        // Fill all 90 ingredient types
        for (int i = 0; i < 90; i++) {
          auto ingredient_type = static_cast<core::food::IngredientType>(i);
          depot.add_ingredient(ingredient_type, quantity, quality, Money{100});
        }

        return CommandResult::success().add_message(
            compat::format("Filled all 90 ingredients with {}g at quality {} "
                           "for player {}'s depot in {}",
                           quantity, quality, player_id.value, city->name()));
      },
  };
}

static CommandDef make_depot_stock_steal() {
  return CommandDef{
      .name = "steal",
      .description = "Steal a random  ingredient stock category",
      .help_text =
          R"(Usage: depot stock steal [city_id] [player_id]

Triggers theft of ingredients from a depot.

Examples:
  depot stock steal             # Steal from active city/player depot
  depot stock steal @ 0         # Active city, player 0
)",
      .args = city_player_args(),
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto city_id = ctx.city_id(0);
        auto player_id = ctx.player_id(1);
        auto* player = ctx.state.player(player_id);

        ctx.state.ensure_city_loaded(city_id);
        auto* city = ctx.state.city(city_id);
        if (!city) {
          return CommandResult::error(
              compat::format("Failed to load city {}", city_id.value));
        }

        auto& depot = city->central_depot(player_id);

        auto category =
            static_cast<IngredientCategory>(engine::random::rand_max_n(4));
        depot.steal_stock(category);
        player->make_insurance_claim(InsuranceType::WAREHOUSE);

        return CommandResult::success().add_message(compat::format(
            "Triggered theft for an ingredient stock from player {}'s "
            "depot in {}",
            player_id.value, city->name()));
      },
  };
}

static CommandDef make_depot_stock_clear() {
  return CommandDef{
      .name = "clear",
      .description = "Clear all ingredient stocks",
      .help_text =
          R"(Usage: depot stock clear [city_id] [player_id]

Removes all ingredient stocks from the depot.

Examples:
  depot stock clear             # Clear active city/player depot
  depot stock clear @ 0         # Active city, player 0
)",
      .args = city_player_args(),
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto city_id = ctx.city_id(0);
        auto player_id = ctx.player_id(1);

        ctx.state.ensure_city_loaded(city_id);
        auto* city = ctx.state.city(city_id);
        if (!city) {
          return CommandResult::error(
              compat::format("Failed to load city {}", city_id.value));
        }

        auto& depot = city->central_depot(player_id);

        // Clear all stocks
        for (int i = 0; i < 90; i++) {
          auto ingredient_type = static_cast<core::food::IngredientType>(i);
          auto& stock = depot.get_stock(ingredient_type);
          stock.quantity = 0;
          stock.weighted_avg_quality = 100;
          stock.weighted_avg_cost = Money{0};
        }

        return CommandResult::success().add_message(
            compat::format("Cleared all ingredient stocks from player {}'s "
                           "depot in {}",
                           player_id.value, city->name()));
      },
  };
}

static CommandDef make_depot_weapons() {
  return CommandDef{
      .name = "weapons",
      .description = "Set a weapon count in the player's depot",
      .help_text =
          R"(Usage: depot weapons <item_id> <amount> [city_id] [player_id] [depot]

Examples:
  depot weapons 6 5             # five bazookas in the active depot
  depot weapons 6 0             # clear them out again
  depot weapons 6 5 @ @ 1       # five in the player's second depot
)",
      .args =
          {
              ArgSpec{
                  .name = "item_id",
                  .type = ArgType::ItemId,
                  .optional = false,
                  .description = "Weapon item ID (0-7)",
              },
              ArgSpec{
                  .name = "amount",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "Quantity (0-255)",
                  .min_value = 0,
                  .max_value = 255,
              },
              ArgSpec{
                  .name = "city_id",
                  .type = ArgType::CityId,
                  .optional = true,
                  .default_provider = DefaultProvider::ActiveCity,
                  .description = "City ID (@ for active city)",
                  .min_value = 0,
                  .max_value = 9,
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
              ArgSpec{
                  .name = "depot",
                  .type = ArgType::Int,
                  .optional = true,
                  .description = "Which of the player's depots in the city",
                  .min_value = 0,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto item_id = ctx.item_id(0);
        auto amount = static_cast<int>(ctx.int_arg(1));
        auto city_id = ctx.city_id(2);
        auto player_id = ctx.player_id(3);
        auto depot_index =
            ctx.args.size() > 4 ? static_cast<int>(ctx.int_arg(4)) : 0;

        if (std::ranges::find(core::item::weapon_list, item_id) ==
            core::item::weapon_list.end()) {
          return CommandResult::error(compat::format(
              "{} is not a weapon", core::item::to_string(item_id)));
        }

        City* city = ctx.state.city(city_id);
        if (!city) {
          return CommandResult::error(
              compat::format("City {} not loaded", city_id.value));
        }

        Property* depot = find_player_depot(*city, player_id, depot_index);
        if (!depot) {
          return CommandResult::error(
              compat::format("Player {} has no depot {} in {}", player_id.value,
                             depot_index, city->name()));
        }

        auto* weapons = depot->weapons();
        if (!weapons) {
          return CommandResult::error("That depot has no weapon store");
        }

        (*weapons)[item_id] = amount;

        return CommandResult::success().add_message(
            compat::format("Set {} to {} in player {}'s depot {} in {}",
                           core::item::to_string(item_id), amount,
                           player_id.value, depot_index, city->name()));
      },
  };
}

static CommandDef make_depot_stock() {
  return CommandDef{
      .name = "stock",
      .description = "Manage depot ingredient stocks",
      .help_text = "Subcommands: add, fill, clear",
  }
      .add_subcommand(make_depot_stock_add())
      .add_subcommand(make_depot_stock_fill())
      .add_subcommand(make_depot_stock_clear())
      .add_subcommand(make_depot_stock_steal());
}

CommandDef make_depot_commands() {
  return CommandDef{
      .name = "depot",
      .aliases = {"d", "dep"},
      .description = "Depot management commands",
      .help_text = "Manage depots: give, info, stock, weapons",
  }
      .add_subcommand(make_depot_give())
      .add_subcommand(make_depot_info())
      .add_subcommand(make_depot_stock())
      .add_subcommand(make_depot_weapons());
}

} // namespace debug::commands
