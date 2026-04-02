/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

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
  void report_previous_day_shortages();
  void do_shopping();
  void do_price_adjustment();
  void do_turnover_monitoring();

  Restaurant& restaurant_;
  GameState& game_state_;
};

} // namespace core::restaurant
