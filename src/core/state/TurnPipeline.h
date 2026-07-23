/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "core/time/TimeOfDay.h"

#include <string_view>
#include <variant>
#include <vector>

// End-of-turn pipeline. GameState::advance_pipeline() drains steps until one
// returns Waiting; that step arranges a callback (popup dismissal, screen pop,
// fade midpoint) which re-enters advance_pipeline() to resume.
namespace turn {

enum class StepResult {
  Done,    // driver continues with the next step
  Waiting, // a callback will re-enter advance_pipeline() later
};

struct ProcessRestaurant {
  static constexpr const char* name = "ProcessRestaurant";
  core::RestaurantId id;
  bool interactive{false};
};

// Lets the status display repaint between restaurants so the player sees
// the money change per restaurant, like the original.
struct Delay {
  static constexpr const char* name = "Delay";
  int frames{0};
};

// Switches to the next player and appends the day/turn boundary steps.
struct FinishTurn {
  static constexpr const char* name = "FinishTurn";
  std::vector<core::RestaurantId> restaurant_ids;
  bool interactive{false};
};

struct TickCities {
  static constexpr const char* name = "TickCities";
};

struct EndOfWeekProcessing {
  static constexpr const char* name = "EndOfWeekProcessing";
};

struct EndOfYearProcessing {
  static constexpr const char* name = "EndOfYearProcessing";
};

struct EndOfMonthProcessing {
  static constexpr const char* name = "EndOfMonthProcessing";
};

struct AnnualCompetition {
  static constexpr const char* name = "AnnualCompetition";
};

struct MonthlyCompetition {
  static constexpr const char* name = "MonthlyCompetition";
};

struct FadeTransition {
  static constexpr const char* name = "FadeTransition";
  int frames{20};
};

struct AdvanceDate {
  static constexpr const char* name = "AdvanceDate";
  core::time::TimeOfDay start_time;
};

struct SetTimeOfDay {
  static constexpr const char* name = "SetTimeOfDay";
  core::time::TimeOfDay start_time;
};

struct PreparePlayerTurn {
  static constexpr const char* name = "PreparePlayerTurn";
};

struct ResumeTime {
  static constexpr const char* name = "ResumeTime";
};

struct CityStartOfDay {
  static constexpr const char* name = "CityStartOfDay";
};

struct PlayerTurnCard {
  static constexpr const char* name = "PlayerTurnCard";
};

struct WeeklyReports {
  static constexpr const char* name = "WeeklyReports";
  core::PlayerId previous_player;
  std::vector<core::RestaurantId> restaurant_ids;
  bool interleave_electricity{false};
};

// End-of-month electricity/slot invoice for one restaurant. Only used when
// it is not also end of week; WeeklyReports interleaves it itself then.
struct MonthlyRestaurantInvoice {
  static constexpr const char* name = "MonthlyRestaurantInvoice";
  core::RestaurantId id;
};

struct MonthlyInvoice {
  static constexpr const char* name = "MonthlyInvoice";
  core::PlayerId previous_player;
};

struct Autosave {
  static constexpr const char* name = "Autosave";
};

// Changes the caption the turn lock layer shows for the following steps.
struct SetActivity {
  static constexpr const char* name = "SetActivity";
  std::string_view label;
};

using Step =
    std::variant<ProcessRestaurant, Delay, FinishTurn, TickCities,
                 EndOfWeekProcessing, EndOfYearProcessing, EndOfMonthProcessing,
                 AnnualCompetition, MonthlyCompetition, FadeTransition,
                 AdvanceDate, SetTimeOfDay, PreparePlayerTurn, ResumeTime,
                 CityStartOfDay, PlayerTurnCard, WeeklyReports,
                 MonthlyRestaurantInvoice, MonthlyInvoice, Autosave,
                 SetActivity>;

inline const char* step_name(const Step& step) {
  return std::visit([]<typename T>(const T&) { return T::name; }, step);
}

} // namespace turn
