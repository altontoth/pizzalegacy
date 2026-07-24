/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include "core/character/Character.h"
#include "core/food/IngredientCategoryType.h"

class GameState;
class Restaurant;

namespace core::restaurant {

// Daily manager routine: runs each game day when a manager is assigned.
// Handles availability/activity gates, shopping, price adjustment,
// turnover monitoring, staff shortage reporting, and staff influence.
class ManagerRoutine {
public:
  ManagerRoutine(Restaurant& restaurant, GameState& game_state);
  void run_daily();

private:
  static int count_category_shortages(const Restaurant& restaurant,
                                      IngredientCategory category);
  void queue_shortage_warning(IngredientCategory category, int shortage_count);

  // A true return means a "Boss! ..." alert went out, which ends the day.
  bool report_previous_day_shortages(core::character::JobRole filled_in_as,
                                     core::character::JobRole shortage);
  void do_shopping();
  void do_price_adjustment();
  bool do_turnover_monitoring();

  // Alerts are raised at day start but held until opening time; the shopping
  // warnings come out one category per 10-minute block after that.
  void queue_alert(std::string message);
  void queue_alert(std::string message, int display_block);

  Restaurant& restaurant_;
  GameState& game_state_;
};

} // namespace core::restaurant
