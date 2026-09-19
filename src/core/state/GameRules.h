/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>

// Market share domination needed to win. The English release checks Paris
// alone, apparently meaning any one city; the German one checks every city.
enum class WinCondition { Paris, AnyCity, AllCities };

inline std::string to_string(WinCondition condition) {
  switch (condition) {
  case WinCondition::Paris:
    return "Paris";
  case WinCondition::AnyCity:
    return "Any city";
  case WinCondition::AllCities:
    return "All cities";
  }
  return "??";
}

inline std::string_view to_config_value(WinCondition condition) {
  switch (condition) {
  case WinCondition::Paris:
    return "paris";
  case WinCondition::AnyCity:
    return "any_city";
  case WinCondition::AllCities:
    return "all_cities";
  }
  return "any_city";
}

inline std::optional<WinCondition>
win_condition_from_config_value(std::string_view value) {
  if (value == "paris") {
    return WinCondition::Paris;
  }
  if (value == "any_city") {
    return WinCondition::AnyCity;
  }
  if (value == "all_cities") {
    return WinCondition::AllCities;
  }
  return std::nullopt;
}

// The options that change how a game plays. A game has its own copy, stored
// in the save game, so the settings get loaded along with the rest.
struct GameRules {
  bool fix_legacy_bugs = true;
  bool qol_improvements = true;
  // The mafia shakes down a random restaurant instead of always the first.
  bool mafia_shakedowns_target_all_restaurants = false;
  bool demand_fulfillment_popularity = true;
  WinCondition win_condition = WinCondition::AnyCity;
  // Off stops the police turning up by themselves. Console only.
  bool police = true;

  bool operator==(const GameRules&) const = default;
};

// How the original game plays, for a career that was played there.
constexpr GameRules original_rules{
    .fix_legacy_bugs = false,
    .qol_improvements = true,
    .mafia_shakedowns_target_all_restaurants = false,
    .demand_fulfillment_popularity = false,
    .win_condition = WinCondition::Paris,
    .police = true,
};
