/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/commands/NavCommands.h"
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
        ctx.state.bootstrap_minimal_playable_world(ctx.context);
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
              ArgSpec{
                  .name = "restaurant_id",
                  .type = ArgType::RestaurantId,
                  .optional = true,
                  .default_provider = DefaultProvider::ActiveRestaurant,
                  .description = "Restaurant ID (@ for active)",
              },
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
          .handler = [](const CommandContext& ctx) -> CommandResult {
            ctx.state.bootstrap_minimal_playable_world(ctx.context);
            ctx.state.ui().push<StockMainState>(StockMainState{});
            return CommandResult::success().add_message(
                "Navigated to stock management");
          },
      });
}

} // namespace debug::commands
