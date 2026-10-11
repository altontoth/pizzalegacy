/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/food/PizzaJudging.h"

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
  // The mafia shakes down a random restaurant instead of always the first.
  bool mafia_shakedowns_target_all_restaurants = false;
  bool demand_fulfillment_popularity = true;
  WinCondition win_condition = WinCondition::AnyCity;
  PizzaJudging pizza_judging = PizzaJudging::Relaxed;
  // A standard pizza's cook book page can be opened in the game.
  bool cook_book = true;
  // Off stops the police turning up by themselves. Console only.
  bool police = true;
  // A supplier holds a missed contract payment against the player in
  // negotiation for a while. The original reads the value but never sets it.
  bool suppliers_remember_missed_payments = false;

  bool operator==(const GameRules&) const = default;
};

// The rules by the names as shown in the options screen.
namespace rule_labels {
constexpr const char* fix_legacy_bugs = "Fix original bugs";
constexpr const char* mafia_shakedowns_target_all_restaurants =
    "Random shakedown target";
constexpr const char* demand_fulfillment_popularity =
    "Fulfillment affects popularity";
constexpr const char* win_condition = "Win condition";
constexpr const char* pizza_judging = "Standard pizza judging";
constexpr const char* cook_book = "In-game cook book";
constexpr const char* police = "Police";
constexpr const char* suppliers_remember_missed_payments =
    "Suppliers remember late payers";
} // namespace rule_labels

// Returns one line for each rule whose value differs, with their value.
std::vector<std::string> rule_differences(const GameRules& theirs,
                                          const GameRules& ours);
