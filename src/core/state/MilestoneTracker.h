/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

class City;
class GameState;
class Player;

// Scans player and city state for achievement milestones and schedules the
// matching Levent messages. Stateless; split out of GameState for cohesion
// and written against its public API.
class MilestoneTracker {
public:
  explicit MilestoneTracker(GameState& state) : gs_(&state) {}

  // Call when a restaurant is opened for the first time.
  // Checks and schedules restaurant count / city milestone events.
  void check_restaurant_milestones(const Player& player);

  // Daily asset/sales/service milestones; weekly profit and delivery
  // milestones when is_end_of_week is set.
  void check_milestone_events(Player& player, bool is_end_of_week);

  // Market share milestones for every player in the given city.
  void check_market_share_milestones(const City& city);

  // Win condition: market share domination (Paris only in the original,
  // all cities with fix_legacy_bugs).
  void check_world_domination();

  // Central depot count milestones.
  void check_depot_milestones(const Player& player);

private:
  GameState* gs_;
};
