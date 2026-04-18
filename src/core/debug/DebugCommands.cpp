/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/DebugCommands.h"

#include "core/debug/command/CommandRegistry.h"
#include "core/debug/commands/ActorCommands.h"
#include "core/debug/commands/CityCommands.h"
#include "core/debug/commands/DebugCommands.h"
#include "core/debug/commands/DepotCommands.h"
#include "core/debug/commands/GameCommands.h"
#include "core/debug/commands/MayorCommands.h"
#include "core/debug/commands/NavCommands.h"
#include "core/debug/commands/NewspaperCommands.h"
#include "core/debug/commands/PlayerCommands.h"
#include "core/debug/commands/RestaurantCommands.h"
#include "core/debug/commands/StaffCommands.h"
#include "core/util/GameContext.h"

void register_debug_commands(DebugConsole& console, GameState& state,
                             GameContext& context) {
  static debug::command::CommandRegistry registry;

  // Register main command trees
  registry.register_command(debug::commands::make_player_commands());
  registry.register_command(debug::commands::make_restaurant_commands());
  registry.register_command(debug::commands::make_depot_commands());
  registry.register_command(debug::commands::make_game_commands());
  registry.register_command(debug::commands::make_nav_commands());
  registry.register_command(debug::commands::make_debug_commands());
  registry.register_command(debug::commands::make_newspaper_commands());
  registry.register_command(debug::commands::make_city_commands());
  registry.register_command(debug::commands::make_mafia_commands());
  registry.register_command(debug::commands::make_police_commands());
  registry.register_command(debug::commands::make_mayor_commands());
  registry.register_command(debug::commands::make_staff_commands());
  registry.register_command(debug::commands::make_armsdealer_commands());

  // Register top-level convenience commands
  registry.register_command(debug::commands::make_dump_command());
  registry.register_command(debug::commands::make_test_command());
  registry.register_command(debug::commands::make_help_command());

  // Initialize console with registry - all command execution delegates to
  // registry
  console.init(registry, state, context);
}
