/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ArgSpec.h"
#include "ArgType.h"
#include "DefaultProvider.h"
#include "core/util/Constants.h"

namespace debug::command {

// The trailing arguments almost every command takes, each falling back to
// what the player is currently looking at.

inline ArgSpec active_player_arg() {
  return ArgSpec{
      .name = "player_id",
      .type = ArgType::PlayerId,
      .optional = true,
      .default_provider = DefaultProvider::ActivePlayer,
      .description = "Player ID (@ for active player)",
      .min_value = 0,
      .max_value = NUMBER_OF_PLAYERS - 1,
  };
}

inline ArgSpec active_restaurant_arg() {
  return ArgSpec{
      .name = "restaurant_id",
      .type = ArgType::RestaurantId,
      .optional = true,
      .default_provider = DefaultProvider::ActiveRestaurant,
      .description = "Restaurant ID (@ for active restaurant)",
  };
}

inline ArgSpec active_city_arg() {
  return ArgSpec{
      .name = "city_id",
      .type = ArgType::CityId,
      .optional = true,
      .default_provider = DefaultProvider::ActiveCity,
      .description = "City ID (@ for active city)",
      .min_value = 0,
      .max_value = NUMBER_OF_CITIES - 1,
  };
}

} // namespace debug::command
