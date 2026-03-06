/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string_view>

enum class ArrestReason {
  Receiving,
  Burglary,
  WeaponsDealing,
  LaunderingMoney,
  ShadyDealings,
  SuspectedHomicide,
  PosessingWeapons,
};

enum class GameMode {
  FREEMODE,
  MISSION_MODE,
  QUICKSTART,
};

constexpr std::string_view to_string(GameMode mode) {
  switch (mode) {
  case GameMode::FREEMODE:
    return "Free mode";
  case GameMode::MISSION_MODE:
    return "Mission mode";
  case GameMode::QUICKSTART:
    return "Quick start";
  }
}

enum class MapMode {
  GAME_INIT,
  TRAVEL,
  DEPOT,
};

enum class TerminalCategory {
  REALTORS = 0,
  BANKS,
  INSURANCE,
  AUTHORITIES,
  OTHERS,
  NONE,
};

enum class PizzaManagerMode {
  Recipes,
  View,
  Ingredients,
  Design,
  Info,
  CompetitorChallenge,
  MonthlyChallenge,
  OwnPizzaJudging,
  CanonicalPizzaJudging,
  CompetitionJudging,
};
