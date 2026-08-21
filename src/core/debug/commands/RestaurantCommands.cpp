/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/commands/RestaurantCommands.h"

#include "core/character/Staff.h"
#include "core/city/Property.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/state/RealEstate.h"
#include "core/util/GameContext.h"
#include "factories/CharacterFactory.h"
#include "factories/PizzaFactory.h"
#include "factories/RestaurantLayoutFactory.h"
#include "resources/Bob.h"
#include "resources/BobSizeFn.h"

using namespace debug::command;

namespace debug::commands {

// Common argument specs for restaurant commands
static std::vector<ArgSpec> restaurant_args() {
  return {
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
          .name = "restaurant_id",
          .type = ArgType::RestaurantId,
          .optional = true,
          .default_provider = DefaultProvider::ActiveRestaurant,
          .description = "Restaurant ID (@ for active restaurant)",
      },
  };
}

static CommandDef make_restaurant_info() {
  return CommandDef{
      .name = "info",
      .description = "Show restaurant information",
      .help_text = R"(Usage: restaurant info [player_id] [restaurant_id]

Displays detailed information about a restaurant including layout,
furniture count, and statistics.

Examples:
  restaurant info              # Active player's active restaurant
  restaurant info @            # Active player's active restaurant (explicit)
  restaurant info @ 0          # Active player's restaurant 0
  restaurant info 2 3          # Player 2's restaurant 3
)",
      .args = restaurant_args(),
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto restaurant_id = ctx.restaurant_id(1);

        auto* restaurant = ctx.state.restaurant(restaurant_id);
        if (!restaurant) {
          return CommandResult::error(
              compat::format("Restaurant {} not found", restaurant_id.value));
        }

        // Check ownership via property
        auto owner = restaurant->property().owner_or_tenant();
        if (!owner || *owner != player_id) {
          return CommandResult::error(
              compat::format("Restaurant {} is not owned by player {}",
                             restaurant_id.value, player_id.value));
        }

        std::vector<std::string> lines;
        lines.push_back(
            compat::format("Restaurant ID: {}", restaurant_id.value));
        lines.push_back(compat::format("Owner: Player {}", player_id.value));
        lines.push_back(compat::format("Name: {}", restaurant->name()));

        auto& layout = restaurant->layout();
        int furniture_count = layout->placed_furniture().size() +
                              layout->placed_kitchen_furniture().size();
        lines.push_back(compat::format("Furniture count: {}", furniture_count));
        lines.push_back(compat::format("Furniture value: ${:n}",
                                       layout->furniture_value()));

        return CommandResult::success(lines);
      },
  };
}

static CommandDef make_restaurant_clear() {
  return CommandDef{
      .name = "clear",
      .description = "Remove all furniture from restaurant",
      .help_text = R"(Usage: restaurant clear [player_id] [restaurant_id]

Removes all furniture from the restaurant layout.

Examples:
  restaurant clear             # Clear active restaurant
  restaurant clear @ 0         # Clear restaurant 0 of active player
  restaurant clear 2 3         # Clear player 2's restaurant 3
)",
      .args = restaurant_args(),
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto restaurant_id = ctx.restaurant_id(1);

        auto* restaurant = ctx.state.restaurant(restaurant_id);
        if (!restaurant) {
          return CommandResult::error(
              compat::format("Restaurant {} not found", restaurant_id.value));
        }

        auto owner = restaurant->property().owner_or_tenant();
        if (!owner || *owner != player_id) {
          return CommandResult::error(
              compat::format("Restaurant {} is not owned by player {}",
                             restaurant_id.value, player_id.value));
        }
        restaurant->layout()->clear();

        // For now, clearing is not implemented as there's no clear() method
        // User can recreate the restaurant layout manually
        return CommandResult::success({"Restaurant furniture removed"});
      },
  };
}

static CommandDef make_restaurant_shutdown() {
  return CommandDef{
      .name = "shutdown",
      .description = "Shut a restaurant down and give up its premise",
      .help_text = R"(Usage: restaurant shutdown [player_id] [restaurant_id]

Sells the premise, or ends the lease if it is rented, which takes the
restaurant with it. The owner is paid nothing for a premise they own.
To stop trading without losing the premise, use 'restaurant close'.

Examples:
  restaurant shutdown          # Shut down the active restaurant
  restaurant shutdown 3 1      # Shut down player 3's restaurant 1
)",
      .args = restaurant_args(),
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto restaurant_id = ctx.restaurant_id(1);

        auto* restaurant = ctx.state.restaurant(restaurant_id);
        if (!restaurant) {
          return CommandResult::error(
              compat::format("Restaurant {} not found", restaurant_id.value));
        }

        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} does not exist", player_id.value));
        }

        auto& property = restaurant->property();
        auto owner = property.owner_or_tenant();
        if (!owner || *owner != player_id) {
          return CommandResult::error(
              compat::format("Restaurant {} is not owned by player {}",
                             restaurant_id.value, player_id.value));
        }

        const bool given_up =
            property.is_rented()
                ? ctx.state.real_estate().end_property_lease(property, *player)
                : ctx.state.real_estate().sell_property(property, *player,
                                                        Money{0});
        if (!given_up) {
          return CommandResult::error(compat::format(
              "Restaurant {} could not be shut down", restaurant_id.value));
        }

        return CommandResult::success().add_message(
            compat::format("Shut down restaurant {}", restaurant_id.value));
      },
  };
}

static CommandDef make_restaurant_open() {
  return CommandDef{
      .name = "open",
      .description = "Open a restaurant for business",
      .help_text = R"(Usage: restaurant open [player_id] [restaurant_id]

Opens the restaurant, so it serves customers within its business hours.
A restaurant an inspection has shut stays shut until its days run out.

Examples:
  restaurant open              # Open the active restaurant
  restaurant open 3 1          # Open player 3's restaurant 1
)",
      .args = restaurant_args(),
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto restaurant_id = ctx.restaurant_id(1);

        auto* restaurant = ctx.state.restaurant(restaurant_id);
        if (!restaurant) {
          return CommandResult::error(
              compat::format("Restaurant {} not found", restaurant_id.value));
        }

        auto owner = restaurant->property().owner_or_tenant();
        if (!owner || *owner != player_id) {
          return CommandResult::error(
              compat::format("Restaurant {} is not owned by player {}",
                             restaurant_id.value, player_id.value));
        }

        if (restaurant->is_force_closed()) {
          return CommandResult::error(compat::format(
              "Restaurant {} is force-closed", restaurant_id.value));
        }

        restaurant->open_for_business(ctx.state);
        return CommandResult::success().add_message(compat::format(
            "Restaurant {} is open for business", restaurant_id.value));
      },
  };
}

static CommandDef make_restaurant_close() {
  return CommandDef{
      .name = "close",
      .description = "Close a restaurant for business (see shutdown to get rid "
                     "of the restaurant)",
      .help_text = R"(Usage: restaurant close [player_id] [restaurant_id]

Closes the restaurant to customers and leaves everything else alone. To give
up the premise as well, use 'restaurant shutdown'.

Examples:
  restaurant close             # Close the active restaurant
  restaurant close 3 1         # Close player 3's restaurant 1
)",
      .args = restaurant_args(),
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto restaurant_id = ctx.restaurant_id(1);

        auto* restaurant = ctx.state.restaurant(restaurant_id);
        if (!restaurant) {
          return CommandResult::error(
              compat::format("Restaurant {} not found", restaurant_id.value));
        }

        auto owner = restaurant->property().owner_or_tenant();
        if (!owner || *owner != player_id) {
          return CommandResult::error(
              compat::format("Restaurant {} is not owned by player {}",
                             restaurant_id.value, player_id.value));
        }

        restaurant->close_for_business();
        return CommandResult::success().add_message(
            compat::format("Restaurant {} is closed", restaurant_id.value));
      },
  };
}

static CommandDef make_restaurant_ai_layout() {
  return CommandDef{
      .name = "ai-layout",
      .description = "Replace a restaurant's layout with a prebuilt AI one",
      .help_text =
          R"(Usage: restaurant ai-layout <index> [player_id] [restaurant_id]

Loads f/f<index>.pc over the restaurant's own layout, furniture and all, so a
prebuilt AI interior can be walked through. The room takes the layout's size,
so leave the restaurant view before running this and go back in afterwards.

Indices 1 to 18 are the ones an AI picks from; 19 to 24 exist as files but are
never used.

Examples:
  restaurant ai-layout 1        # Active restaurant gets the first AI layout
  restaurant ai-layout 19 @ 0   # Restaurant 0, one of the unreachable layouts
)",
      .args =
          {
              ArgSpec{
                  .name = "index",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "AI layout number",
                  .min_value = 1,
                  .max_value = RestaurantLayoutFactory::ai_layout_count,
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
                  .name = "restaurant_id",
                  .type = ArgType::RestaurantId,
                  .optional = true,
                  .default_provider = DefaultProvider::ActiveRestaurant,
                  .description = "Restaurant ID (@ for active restaurant)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        const int index = static_cast<int>(ctx.int_arg(0));
        auto player_id = ctx.player_id(1);
        auto restaurant_id = ctx.restaurant_id(2);

        auto* restaurant = ctx.state.restaurant(restaurant_id);
        if (!restaurant) {
          return CommandResult::error(
              compat::format("Restaurant {} not found", restaurant_id.value));
        }

        auto owner = restaurant->property().owner_or_tenant();
        if (!owner || *owner != player_id) {
          return CommandResult::error(
              compat::format("Restaurant {} is not owned by player {}",
                             restaurant_id.value, player_id.value));
        }

        // More furniture than any layout file holds, so none of it is dropped.
        constexpr int every_item = 255;
        if (!restaurant->ai_layout(index, every_item)) {
          return CommandResult::error(
              compat::format("AI layout {} could not be loaded", index));
        }

        const auto size = restaurant->size();
        return CommandResult::success().add_message(
            compat::format("Restaurant {} now has AI layout {} ({}x{})",
                           restaurant_id.value, index, size.w, size.h));
      },
  };
}

static CommandDef make_restaurant_furniture_list() {
  return CommandDef{
      .name = "list",
      .description = "List all furniture in restaurant",
      .help_text =
          R"(Usage: restaurant furniture list [player_id] [restaurant_id]

Lists all furniture items placed in the restaurant.

Examples:
  restaurant furniture list        # Active restaurant
  restaurant furniture list @ 0    # Restaurant 0 of active player
)",
      .args = restaurant_args(),
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto restaurant_id = ctx.restaurant_id(1);

        auto* restaurant = ctx.state.restaurant(restaurant_id);
        if (!restaurant) {
          return CommandResult::error(
              compat::format("Restaurant {} not found", restaurant_id.value));
        }

        auto owner = restaurant->property().owner_or_tenant();
        if (!owner || *owner != player_id) {
          return CommandResult::error(
              compat::format("Restaurant {} is not owned by player {}",
                             restaurant_id.value, player_id.value));
        }

        auto& layout = restaurant->layout();
        int furniture_count = layout->placed_furniture().size() +
                              layout->placed_kitchen_furniture().size();
        std::vector<std::string> lines;
        lines.push_back(
            compat::format("Furniture in restaurant {}:", restaurant_id.value));
        lines.push_back(compat::format("Total items: {}", furniture_count));
        lines.push_back(
            compat::format("Total value: ${:n}", layout->furniture_value()));

        return CommandResult::success(lines);
      },
  };
}

static CommandDef make_restaurant_furniture() {
  return CommandDef{
      .name = "furniture",
      .description = "Manage restaurant furniture",
      .help_text = "Subcommands: list",
  }
      .add_subcommand(make_restaurant_furniture_list());
}

static CommandDef make_restaurant_stock_add() {
  return CommandDef{
      .name = "add",
      .description = "Add stock for a specific ingredient",
      .help_text =
          R"(Usage: restaurant stock add <ingredient_id> [quantity] [quality] [player_id] [restaurant_id]

Adds stock for a specific ingredient type.

Arguments:
  ingredient_id - Ingredient ID (0-89)
  quantity      - Quantity in grams (default: 10000)
  quality       - Quality 0-100 (default: 100)
  player_id     - Player ID (@ for active player)
  restaurant_id - Restaurant ID (@ for active restaurant)

Examples:
  restaurant stock add 0              # Add 10kg of ingredient 0 at quality 100
  restaurant stock add 5 5000         # Add 5kg of ingredient 5 at quality 100
  restaurant stock add 10 20000 80    # Add 20kg of ingredient 10 at quality 80
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
                  .name = "player_id",
                  .type = ArgType::PlayerId,
                  .optional = true,
                  .default_provider = DefaultProvider::ActivePlayer,
                  .description = "Player ID (@ for active player)",
                  .min_value = 0,
                  .max_value = NUMBER_OF_PLAYERS - 1,
              },
              ArgSpec{
                  .name = "restaurant_id",
                  .type = ArgType::RestaurantId,
                  .optional = true,
                  .default_provider = DefaultProvider::ActiveRestaurant,
                  .description = "Restaurant ID (@ for active restaurant)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto ingredient_id = ctx.int_arg(0);
        auto quantity = ctx.args.size() > 1 ? ctx.int_arg(1) : 10000;
        auto quality = ctx.args.size() > 2 ? ctx.int_arg(2) : 100;
        auto player_id = ctx.player_id(3);
        auto restaurant_id = ctx.restaurant_id(4);

        auto* restaurant = ctx.state.restaurant(restaurant_id);
        if (!restaurant) {
          return CommandResult::error(
              compat::format("Restaurant {} not found", restaurant_id.value));
        }

        auto owner = restaurant->property().owner_or_tenant();
        if (!owner || *owner != player_id) {
          return CommandResult::error(
              compat::format("Restaurant {} is not owned by player {}",
                             restaurant_id.value, player_id.value));
        }

        auto ingredient_type =
            static_cast<core::food::IngredientType>(ingredient_id);
        restaurant->inventory().add_ingredient(ingredient_type, quantity,
                                               quality, Money{100});

        return CommandResult::success().add_message(compat::format(
            "Added {}g of ingredient {} at quality {} to restaurant {}",
            quantity, ingredient_id, quality, restaurant_id.value));
      },
  };
}

static CommandDef make_restaurant_stock_fill() {
  return CommandDef{
      .name = "fill",
      .description = "Fill all ingredient stocks to specified quantity",
      .help_text =
          R"(Usage: restaurant stock fill <quantity> <quality> [player_id] [restaurant_id]

Fills all ingredient stocks to the specified quantity and quality.

Arguments:
  quantity      - Quantity in kilogram per ingredient
  quality       - Quality 0-100
  player_id     - Player ID (@ for active player)
  restaurant_id - Restaurant ID (@ for active restaurant)

Examples:
  restaurant stock fill 10 100    # Fill all to 10kg at quality 100
  restaurant stock fill 20 100    # Fill all to 20kg at quality 100
  restaurant stock fill 15 80     # Fill all to 15kg at quality 80
)",
      .args =
          {
              ArgSpec{
                  .name = "quantity",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "Quantity in kilogram",
                  .min_value = 0,
                  .max_value = 650,
              },
              ArgSpec{
                  .name = "quality",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "Quality 0-100",
                  .min_value = 0,
                  .max_value = 100,
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
                  .name = "restaurant_id",
                  .type = ArgType::RestaurantId,
                  .optional = true,
                  .default_provider = DefaultProvider::ActiveRestaurant,
                  .description = "Restaurant ID (@ for active restaurant)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto quantity = ctx.int_arg(0) * 1000;
        auto quality = ctx.int_arg(1);
        auto player_id = ctx.player_id(2);
        auto restaurant_id = ctx.restaurant_id(3);

        auto* restaurant = ctx.state.restaurant(restaurant_id);
        if (!restaurant) {
          return CommandResult::error(
              compat::format("Restaurant {} not found", restaurant_id.value));
        }

        auto owner = restaurant->property().owner_or_tenant();
        if (!owner || *owner != player_id) {
          return CommandResult::error(
              compat::format("Restaurant {} is not owned by player {}",
                             restaurant_id.value, player_id.value));
        }

        // Fill all 90 ingredient types
        for (int i = 0; i < 90; i++) {
          auto ingredient_type = static_cast<core::food::IngredientType>(i);
          restaurant->inventory().add_ingredient(ingredient_type, quantity,
                                                 quality, Money{100});
        }

        return CommandResult::success().add_message(
            compat::format("Filled all 90 ingredients with {}g at quality {} "
                           "for restaurant {}",
                           quantity, quality, restaurant_id.value));
      },
  };
}

static CommandDef make_restaurant_stock_clear() {
  return CommandDef{
      .name = "clear",
      .description = "Clear all ingredient stocks",
      .help_text =
          R"(Usage: restaurant stock clear [player_id] [restaurant_id]

Removes all ingredient stocks from the restaurant.

Examples:
  restaurant stock clear             # Clear active restaurant
  restaurant stock clear @ 0         # Clear restaurant 0 of active player
)",
      .args = restaurant_args(),
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto player_id = ctx.player_id(0);
        auto restaurant_id = ctx.restaurant_id(1);

        auto* restaurant = ctx.state.restaurant(restaurant_id);
        if (!restaurant) {
          return CommandResult::error(
              compat::format("Restaurant {} not found", restaurant_id.value));
        }

        auto owner = restaurant->property().owner_or_tenant();
        if (!owner || *owner != player_id) {
          return CommandResult::error(
              compat::format("Restaurant {} is not owned by player {}",
                             restaurant_id.value, player_id.value));
        }

        // Clear all stocks by setting quantity to 0
        for (int i = 0; i < 90; i++) {
          auto ingredient_type = static_cast<core::food::IngredientType>(i);
          auto& stock = restaurant->inventory().get_stock(ingredient_type);
          stock.quantity = 0;
          stock.weighted_avg_quality = 100;
          stock.weighted_avg_cost = Money{0};
        }

        return CommandResult::success(
            {"Cleared all ingredient stocks from restaurant"});
      },
  };
}

static CommandDef make_restaurant_stock() {
  return CommandDef{
      .name = "stock",
      .description = "Manage restaurant ingredient stocks",
      .help_text = "Subcommands: add, fill, clear",
  }
      .add_subcommand(make_restaurant_stock_add())
      .add_subcommand(make_restaurant_stock_fill())
      .add_subcommand(make_restaurant_stock_clear());
}

static CommandDef make_restaurant_create() {
  return CommandDef{
      .name = "create",
      .description = "Create a new restaurant with layout",
      .help_text =
          R"(Usage: restaurant create <city_id> <player_id> <layout_id>

Creates a new restaurant for the specified player in the given city with a
specific layout. The restaurant will be placed in a random available property.

Arguments:
  city_id    - City ID (@ for active city, 0-9)
  player_id  - Player ID (@ for active player, 0-7)
  layout_id  - Layout number (1,3,5,7 are big; 2,4,6,8 are small)

Examples:
  restaurant create @ @ 1      # Active city/player, layout 1
  restaurant create 0 0 2      # City 0, player 0, layout 2
  restaurant create 6 1 5      # Madrid, player 1, layout 5
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
                  .name = "layout_id",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "Layout ID (1-8)",
                  .min_value = 1,
                  .max_value = 8,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto city_id = ctx.city_id(0);
        auto player_id = ctx.player_id(1);
        auto layout_id = ctx.int_arg(2);

        // Ensure city is loaded
        ctx.state.ensure_city_loaded(city_id);
        auto* city = ctx.state.city(city_id);
        if (!city) {
          return CommandResult::error(
              compat::format("Failed to load city {}", city_id.value));
        }

        // Get player
        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} does not exist", player_id.value));
        }

        // Create the restaurant (will find a random available property)
        auto* restaurant =
            ctx.state.real_estate().create_restaurant_with_layout(
                *player, *city, static_cast<int>(layout_id));

        if (!restaurant) {
          return CommandResult::error(compat::format(
              "Failed to create restaurant with layout {} in city {} (no "
              "available properties or layout not found)",
              layout_id, city_id.value));
        }

        return CommandResult::success().add_message(compat::format(
            "Created restaurant (ID {}) for player {} in "
            "city {} with layout {}",
            restaurant->id().value, player_id.value, city_id.value, layout_id));
      },
  };
}

static CommandDef make_restaurant_qs() {
  return CommandDef{
      .name = "qs",
      .description = "Quick start: Create fully functional restaurant",
      .help_text =
          R"(Usage: restaurant qs <city_id> <player_id>

Creates a fully functional small restaurant with:
- Layout 1 (small restaurant)
- All canonical recipes loaded and added to menu at $8
- One chef hired
- One waiter hired
- All ingredients stocked with 500kg at quality 100
- Restaurant opened and ready to operate

This is useful for quickly setting up restaurants during development.

Arguments:
  city_id    - City ID (@ for active city, 0-9)
  player_id  - Player ID (@ for active player, 0-7)

Examples:
  restaurant qs @ @       # Active city/player
  restaurant qs 0 0       # City 0, player 0
  restaurant qs 3 1       # Paris, player 1
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
        auto city_id = ctx.city_id(0);
        auto player_id = ctx.player_id(1);

        CommandResult result = CommandResult::success();

        // Ensure city is loaded
        ctx.state.ensure_city_loaded(city_id);
        auto* city = ctx.state.city(city_id);
        if (!city) {
          return CommandResult::error(
              compat::format("Failed to load city {}", city_id.value));
        }

        // Get player
        auto* player = ctx.state.player(player_id);
        if (!player) {
          return CommandResult::error(
              compat::format("Player {} does not exist", player_id.value));
        }

        // Ensure there is a valid active city
        if (!is_valid_city_id(ctx.state.active_city_id())) {
          ctx.state.active_city(city_id);
        }

        // Create small restaurant with layout 1
        auto* restaurant =
            ctx.state.real_estate().create_restaurant_with_layout(*player,
                                                                  *city, 1);

        if (!restaurant) {
          return CommandResult::error(compat::format(
              "Failed to create restaurant in city {} (no available "
              "properties or layout not found)",
              city_id.value));
        }

        result.add_message(compat::format(
            "Created small restaurant (ID {}) for player {} in city {}",
            restaurant->id().value, player_id.value, city_id.value));

        // So a following command can reach it as '@'.
        ctx.state.active_restaurant(restaurant->id());

        // Load canonical recipes for the player
        auto& recipes = player->recipes();
        int loaded = 0;

        auto& zutaten_bob = ctx.context.bob->get("bob/zutaten.bob");
        auto get_bob_size =
            make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob);

        for (int i = 0; i < core::food::CANON_COUNT; ++i) {
          const auto cid = static_cast<core::food::CanonicalPizza>(i);
          auto pizza = PizzaFactory::create_pizza(
              cid, get_bob_size, ctx.state.customer_taste_profiles());

          if (!pizza) {
            continue;
          }

          auto rid = recipes.create_from_canonical(cid, std::move(*pizza));
          loaded++;

          // Add to menu at $8
          restaurant->menu().add_or_set(rid, Money{800});
        }

        result.add_message(compat::format(
            "Loaded {} canonical recipes and added to menu at $8", loaded));

        // Hire a chef (using character ID 0)
        auto* chef_data = CharacterFactory::create_character(0);
        core::character::Character chef_character(chef_data);
        Staff chef(core::character::JobRole::CHEF, chef_character);
        chef.payment(Money{static_cast<int64_t>(chef_data->starting_payment(
                               core::character::JobRole::CHEF)) *
                           100});

        HiringResult chef_result = restaurant->add_staff(chef);
        if (chef_result == HiringResult::Success) {
          result.add_message(compat::format("Hired chef: {} {}",
                                            chef_data->first_name,
                                            chef_data->last_name));
        }
        else {
          result.add_message("Failed to hire chef");
        }

        // Hire a waiter (using character ID 1)
        auto* waiter_data = CharacterFactory::create_character(1);
        core::character::Character waiter_character(waiter_data);
        Staff waiter(core::character::JobRole::WAITER, waiter_character);
        waiter.payment(Money{static_cast<int64_t>(waiter_data->starting_payment(
                                 core::character::JobRole::WAITER)) *
                             100});

        HiringResult waiter_result = restaurant->add_staff(waiter);
        if (waiter_result == HiringResult::Success) {
          result.add_message(compat::format("Hired waiter: {} {}",
                                            waiter_data->first_name,
                                            waiter_data->last_name));
        }
        else {
          result.add_message("Failed to hire waiter");
        }

        // Fill ingredient stock (500 kg = 500,000 grams at quality 100)
        for (int i = 0; i < 90; i++) {
          auto ingredient_type = static_cast<core::food::IngredientType>(i);
          restaurant->inventory().add_ingredient(ingredient_type, 500000, 100,
                                                 Money{100});
        }
        result.add_message("Filled all ingredients with 500kg at quality 100");

        // Open the restaurant
        if (restaurant->is_force_closed()) {
          result.add_message("Restaurant is force-closed, cannot open");
        }
        else {
          restaurant->open_for_business(ctx.state);
          result.add_message("Restaurant opened successfully");
        }

        return result;
      },
  };
}

static CommandDef make_restaurant_inspection_timer() {
  return CommandDef{
      .name = "inspection-timer",
      .description = "Set the mayor inspection countdown on a restaurant",
      .help_text =
          R"(Usage: restaurant inspection-timer <days> [player_id] [restaurant_id]

Sets the number of days until the next scheduled mayor inspection.
The inspection fires naturally when the counter reaches zero at end-of-day.
Use 'actor mayor inspect' to trigger one immediately without waiting.

Examples:
  restaurant inspection-timer 1      # Inspection fires next end-of-day
  restaurant inspection-timer 0 @ 2  # Restaurant 2, fires immediately next EOD
)",
      .args =
          {
              ArgSpec{
                  .name = "days",
                  .type = ArgType::Int,
                  .optional = false,
                  .description = "Days until next inspection (0 = next EOD)",
                  .min_value = 0,
                  .max_value = 365,
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
                  .name = "restaurant_id",
                  .type = ArgType::RestaurantId,
                  .optional = true,
                  .default_provider = DefaultProvider::ActiveRestaurant,
                  .description = "Restaurant ID (@ for active restaurant)",
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        int days = static_cast<int>(ctx.int_arg(0));
        auto player_id = ctx.player_id(1);
        auto restaurant_id = ctx.restaurant_id(2);

        auto* restaurant = ctx.state.restaurant(restaurant_id);
        if (!restaurant) {
          return CommandResult::error(
              compat::format("Restaurant {} not found", restaurant_id.value));
        }

        auto owner = restaurant->property().owner_or_tenant();
        if (!owner || *owner != player_id) {
          return CommandResult::error(
              compat::format("Restaurant {} is not owned by player {}",
                             restaurant_id.value, player_id.value));
        }

        restaurant->inspection_days_remaining(days);
        return CommandResult::success().add_message(compat::format(
            "Set inspection timer to {} day(s) for restaurant {}", days,
            restaurant_id.value));
      },
  };
}

CommandDef make_restaurant_commands() {
  return CommandDef{
      .name = "restaurant",
      .aliases = {"r", "rest"},
      .description = "Restaurant management commands",
      .help_text =
          "Manage restaurants: info, clear, open, close, shutdown, ai-layout, "
          "furniture, stock, create, qs, inspection-timer",
  }
      .add_subcommand(make_restaurant_info())
      .add_subcommand(make_restaurant_clear())
      .add_subcommand(make_restaurant_open())
      .add_subcommand(make_restaurant_close())
      .add_subcommand(make_restaurant_shutdown())
      .add_subcommand(make_restaurant_ai_layout())
      .add_subcommand(make_restaurant_furniture())
      .add_subcommand(make_restaurant_stock())
      .add_subcommand(make_restaurant_create())
      .add_subcommand(make_restaurant_qs())
      .add_subcommand(make_restaurant_inspection_timer());
}

} // namespace debug::commands
