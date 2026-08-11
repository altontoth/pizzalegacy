/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/debug/command/DefaultProvider.h"
#include <magic_enum/magic_enum.hpp>
#include "core/debug/command/ParsedValue.h"
#include "core/state/GameState.h"
#include "engine/Fatal.h"

namespace debug::command {

ParsedValue resolve_default(DefaultProvider provider, ArgType type,
                            GameState& state) {
  // Validate compatibility
  if (!is_compatible(provider, type)) {
    fatal(compat::format("Default provider {} is not compatible with type {}",
                         magic_enum::enum_name(provider), type_name(type)));
  }

  ParsedValue result;
  result.type = type;
  result.source = ValueSource::DefaultProvider;
  result.provider_used = provider;
  result.original_token = std::string{"@"};

  switch (provider) {
  case DefaultProvider::None:
    fatal("Cannot resolve None default provider");

  case DefaultProvider::ActivePlayer: {
    auto* player = state.active_player();
    if (!player) {
      fatal("No active player set");
    }
    result.value = player->id();
    break;
  }

  case DefaultProvider::ActiveRestaurant: {
    auto* restaurant = state.active_restaurant();
    if (!restaurant) {
      // During development, if no restaurant exists yet, default to restaurant
      // 0 The command handler will bootstrap if needed
      result.value = core::RestaurantId{0};
    }
    else {
      result.value = restaurant->id();
    }
    break;
  }

  case DefaultProvider::ActiveCity: {
    result.value = state.default_city_id();
    break;
  }

  case DefaultProvider::ActivePizza:
    // Future implementation when pizza designer has active pizza concept
    fatal("ActivePizza default not yet implemented");

  case DefaultProvider::Zero:
    if (type == ArgType::Int) {
      result.value = int64_t{0};
    }
    else if (type == ArgType::Money) {
      result.value = Money{0};
    }
    else {
      fatal("Zero default only valid for Int or Money");
    }
    break;

  case DefaultProvider::EmptyString:
    result.value = std::string{};
    break;

  case DefaultProvider::CurrentDate:
    result.value = state.time().current_date();
    break;
  }

  return result;
}

} // namespace debug::command
