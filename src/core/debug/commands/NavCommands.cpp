/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/commands/NavCommands.h"
#include "core/debug/command/CommonArgs.h"
#include "core/food/IngredientCategoryType.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "engine/Logger.h"

using namespace debug::command;

namespace debug::commands {

// Helper to create simple screen navigation commands
static CommandDef make_screen_nav(const std::string& name,
                                  const std::vector<std::string>& aliases,
                                  const std::string& desc,
                                  ScreenTag::Kind screen) {
  return CommandDef{
      .name = name,
      .aliases = aliases,
      .description = desc,
      .help_text = compat::format(
          "Usage: nav {}\n\nNavigates to the {} screen.", name, desc),
      .handler = [screen](const CommandContext& ctx) -> CommandResult {
        ctx.state.ui().push<ScreenTag>(ScreenTag::Kind{screen});
        return CommandResult::success().add_message(
            compat::format("Navigated to {}", static_cast<int>(screen)));
      },
  };
}

static CommandDef make_nav_cityview() {
  return CommandDef{
      .name = "cityview",
      .aliases = {"city", "cv"},
      .description = "Navigate to city view",
      .help_text = R"(Usage: nav cityview <city_id>

Opens the city view for the specified city.

Examples:
  nav cityview 0       # View city 0
  nav cityview @       # View active city
  nav city 3           # 'city' is an alias
)",
      .args =
          {
              active_city_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto city_id = ctx.city_id(0);
        ctx.state.bootstrap_minimal_playable_world(ctx.context);
        ctx.state.active_city(city_id);
        if (auto* player = ctx.state.active_player()) {
          ctx.state.on_city_entered(city_id, *player);
        }
        ctx.state.ui().replace_top<CityViewState>(CityViewState{city_id.value});
        return CommandResult::success().add_message(
            compat::format("Navigated to city view (city {})", city_id.value));
      },
  };
}

static CommandDef make_nav_restaurant() {
  return CommandDef{
      .name = "restaurant",
      .aliases = {"rest", "rv"},
      .description = "Navigate to restaurant view",
      .help_text = R"(Usage: nav restaurant <restaurant_id>

Opens the restaurant view for the specified restaurant.

Examples:
  nav restaurant @     # View active restaurant
  nav restaurant 0     # View restaurant 0
  nav rest 3           # 'rest' is an alias
)",
      .args =
          {
              active_restaurant_arg(),
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto rid = ctx.restaurant_id(0);
        ctx.state.bootstrap_minimal_playable_world(ctx.context);
        ctx.state.ui().push<RestaurantViewState>(RestaurantViewState{
            .id = rid, .mode = RestaurantViewMode::PLAYER_DEFAULT});
        return CommandResult::success().add_message(compat::format(
            "Navigated to restaurant view (restaurant {})", rid.value));
      },
  };
}

static CommandDef make_nav_stats() {
  return CommandDef{
      .name = "stats",
      .description = "Navigate to a restaurant's statistics screens",
      .help_text = R"(Usage: nav stats <restaurant_id> [screen]

Opens one of a restaurant's statistics screens. They are numbered in the
order their buttons appear: 0 daily balance, 1 week review, 2 month review,
3 week's takings, 4 customer statistics, 5 sales curve.

Examples:
  nav stats @        # Daily balance for the active restaurant
  nav stats @ 4      # Customer statistics
)",
      .args =
          {
              active_restaurant_arg(),
              ArgSpec{
                  .name = "screen",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Which statistics screen to open",
                  .min_value = 0,
                  .max_value = 5,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto rid = ctx.restaurant_id(0);
        std::optional<int> screen;
        if (ctx.args.size() > 1) {
          screen = static_cast<int>(ctx.int_arg(1));
        }
        ctx.state.bootstrap_minimal_playable_world(ctx.context);
        ctx.state.ui().push<RestaurantStatsState>(
            RestaurantStatsState{.id = rid, .screen_index = screen});
        return CommandResult::success().add_message(compat::format(
            "Navigated to statistics (restaurant {}, screen {})", rid.value,
            screen ? std::to_string(*screen) : "default"));
      },
  };
}

static CommandDef make_nav_globalstats() {
  return CommandDef{
      .name = "globalstats",
      .aliases = {"gs"},
      .description = "Navigate to the company statistics screens",
      .help_text = R"(Usage: nav globalstats [screen]

Opens one of the company statistics screens. They are numbered in the order
their buttons appear: 0 month overview, 1 restaurants, 2 cash flow, 3 loans,
4 insurance, 5 property, 6 balance, 7 daily profits.

Examples:
  nav globalstats
  nav globalstats 6
)",
      .args =
          {
              ArgSpec{
                  .name = "screen",
                  .type = ArgType::Int,
                  .optional = true,
                  .default_provider = DefaultProvider::None,
                  .description = "Which statistics screen to open",
                  .min_value = 0,
                  .max_value = 7,
              },
          },
      .handler = [](const CommandContext& ctx) -> CommandResult {
        std::optional<int> screen;
        if (!ctx.args.empty()) {
          screen = static_cast<int>(ctx.int_arg(0));
        }
        ctx.state.bootstrap_minimal_playable_world(ctx.context);
        // The restaurants screen has a UI state of its own.
        if (screen == 1) {
          ctx.state.ui().push<StatsRestaurantsState>(StatsRestaurantsState{
              .player_id = ctx.state.active_player()->id()});
        }
        else {
          ctx.state.ui().push<GlobalStatsState>(
              GlobalStatsState{.screen_index = screen});
        }
        return CommandResult::success().add_message(
            compat::format("Navigated to company statistics (screen {})",
                           screen ? std::to_string(*screen) : "default"));
      },
  };
}

static CommandDef make_nav_profile() {
  return CommandDef{
      .name = "profile",
      .description = "Navigate to a player's profile",
      .help_text = R"(Usage: nav profile [player_id]

Examples:
  nav profile @
  nav profile 1
)",
      .args = {active_player_arg()},
      .handler = [](const CommandContext& ctx) -> CommandResult {
        auto pid = ctx.player_id(0);
        ctx.state.bootstrap_minimal_playable_world(ctx.context);
        ctx.state.ui().push<PlayerProfileState>(
            PlayerProfileState{.player_id = pid});
        return CommandResult::success().add_message(
            compat::format("Navigated to profile (player {})", pid.value));
      },
  };
}

// Shared by the stock and depot screens: a category name, or none for the
// screen that lists them all.
static ArgSpec ingredient_category_arg() {
  return ArgSpec{
      .name = "category",
      .type = ArgType::String,
      .optional = true,
      .default_provider = DefaultProvider::EmptyString,
      .description = "vegetables, meat, fruit or cheese",
      .allowed_values =
          std::vector<std::string>{"vegetables", "meat", "fruit", "cheese"},
  };
}

static std::optional<IngredientCategory>
parse_ingredient_category(const std::string& name) {
  for (auto cat : ALL_INGREDIENT_CATEGORIES) {
    if (to_string(cat) == name) {
      return cat;
    }
  }
  return std::nullopt;
}

static CommandDef make_nav_depot() {
  return CommandDef{
      .name = "depot",
      .description = "Navigate to the central depot",
      .help_text = R"(Usage: nav depot [category]

Opens the central depot of the active city. With a category it opens that
category's stock page instead of the depot's front screen.

Examples:
  nav depot
  nav depot cheese
)",
      .args = {ingredient_category_arg()},
      .handler = [](const CommandContext& ctx) -> CommandResult {
        ctx.state.bootstrap_minimal_playable_world(ctx.context);
        const std::string name = ctx.args.empty() ? "" : ctx.string_arg(0);
        if (name.empty()) {
          ctx.state.ui().push<DepotMainState>(DepotMainState{});
          return CommandResult::success().add_message("Navigated to the depot");
        }
        auto category = parse_ingredient_category(name);
        if (!category) {
          return CommandResult::error(
              compat::format("No such ingredient category: {}", name));
        }
        ctx.state.ui().push<DepotIngredientCategoryViewState>(
            DepotIngredientCategoryViewState{.category = *category});
        return CommandResult::success().add_message(
            compat::format("Navigated to depot stock ({})", name));
      },
  };
}

CommandDef make_nav_commands() {
  return CommandDef{
      .name = "nav",
      .aliases = {"ns"},
      .description = "Navigation commands",
      .help_text = "Navigate to different screens in the game",
  } // Menus
      .add_subcommand(make_screen_nav("intro", {"is"}, "intro screen",
                                      ScreenTag::Kind::Intro))
      .add_subcommand(make_screen_nav("main", {"mm"}, "main menu",
                                      ScreenTag::Kind::MenuMain))
      .add_subcommand(make_screen_nav("ng", {}, "new game menu",
                                      ScreenTag::Kind::MenuNewGame))
      .add_subcommand(make_screen_nav("op", {}, "options menu",
                                      ScreenTag::Kind::MenuOptions))
      .add_subcommand(make_screen_nav("disk", {"dm"}, "disk menu",
                                      ScreenTag::Kind::DiskMenu))

      // Screens
      .add_subcommand(make_screen_nav("advertising", {"ad"}, "advertising",
                                      ScreenTag::Kind::Advertising))

      // Views with arguments
      .add_subcommand(make_nav_cityview())
      .add_subcommand(make_nav_restaurant())
      .add_subcommand(make_nav_stats())
      .add_subcommand(make_nav_globalstats())
      .add_subcommand(make_nav_profile())

      // Pizza manager screens
      .add_subcommand(
          CommandDef{
              .name = "pizza",
              .description = "Pizza designer/recipes",
              .help_text = "Usage: nav pizza <design|recipes>",
          }
              .add_subcommand(CommandDef{
                  .name = "design",
                  .description = "Pizza design screen",
                  .handler = [](const CommandContext& ctx) -> CommandResult {
                    ctx.state.ui().push<PizzaDesignState>(
                        PizzaDesignMode::Design);
                    return CommandResult::success().add_message(
                        "Navigated to pizza designer");
                  },
              })
              .add_subcommand(CommandDef{
                  .name = "recipes",
                  .description = "Recipe list screen",
                  .handler = [](const CommandContext& ctx) -> CommandResult {
                    ctx.state.bootstrap_minimal_playable_world(ctx.context);
                    ctx.state.ui().push<PizzaDesignState>(
                        PizzaDesignMode::Recipes);
                    return CommandResult::success().add_message(
                        "Navigated to recipe list");
                  },
              }))

      // Other screens
      .add_subcommand(CommandDef{
          .name = "euromap",
          .aliases = {"em"},
          .description = "Europe map",
          .handler = [](const CommandContext& ctx) -> CommandResult {
            ctx.state.ui().push<EuroMapState>(EuroMapState{MapMode::TRAVEL});
            return CommandResult::success().add_message(
                "Navigated to Europe map");
          },
      })
      .add_subcommand(CommandDef{
          .name = "terminal",
          .aliases = {"tm"},
          .description = "Terminal",
          .handler = [](const CommandContext& ctx) -> CommandResult {
            ctx.state.ui().push<TerminalState>(std::nullopt);
            return CommandResult::success().add_message(
                "Navigated to Terminal");
          },
      })
      .add_subcommand(CommandDef{
          .name = "purchasing",
          .aliases = {"pm"},
          .description = "Purchasing screen",
          .handler = [](const CommandContext& ctx) -> CommandResult {
            ctx.state.ui().push<PurchasingState>(
                PurchasingState{SupplierType::FURNITURE});
            return CommandResult::success().add_message(
                "Navigated to purchasing");
          },
      })
      .add_subcommand(CommandDef{
          .name = "suppliers",
          .aliases = {"sl"},
          .description = "Supplier list",
          .handler = [](const CommandContext& ctx) -> CommandResult {
            ctx.state.ui().push<SupplierListState>(
                SupplierListState{SupplierType::FURNITURE});
            return CommandResult::success().add_message(
                "Navigated to supplier list");
          },
      })
      .add_subcommand(CommandDef{
          .name = "stock",
          .aliases = {"st"},
          .description = "Stock management",
          .help_text = R"(Usage: nav stock [category]

Opens a restaurant's stock screen. With a category it opens that category's
page instead of the screen that lists them all.

Examples:
  nav stock
  nav stock meat
)",
          .args = {ingredient_category_arg()},
          .handler = [](const CommandContext& ctx) -> CommandResult {
            ctx.state.bootstrap_minimal_playable_world(ctx.context);
            const std::string name = ctx.args.empty() ? "" : ctx.string_arg(0);
            if (name.empty()) {
              ctx.state.ui().push<StockMainState>(StockMainState{});
              return CommandResult::success().add_message(
                  "Navigated to stock management");
            }
            auto category = parse_ingredient_category(name);
            if (!category) {
              return CommandResult::error(
                  compat::format("No such ingredient category: {}", name));
            }
            ctx.state.ui().push<IngredientCategoryViewState>(
                IngredientCategoryViewState{.category = *category});
            return CommandResult::success().add_message(
                compat::format("Navigated to stock ({})", name));
          },
      })
      .add_subcommand(make_nav_depot());
}

} // namespace debug::commands
