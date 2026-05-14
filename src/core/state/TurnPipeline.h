/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "core/time/TimeOfDay.h"

#include <vector>

// This is our end-of-turn pipeline; we have both synchronous and asynchronous
// steps. Sync means that advance_pipeline is called for you when the work is
// complete, and async means you have to call it yourself.
enum class TurnPipelineStep {
  // Sync
  TickCities,
  EndOfWeekProcessing,
  EndOfYearProcessing,
  EndOfMonthProcessing,
  AdvanceDate,
  SetTimeOfDay,
  PreparePlayerTurn,
  ResumeTime,
  PlayerTurnCard,
  CityStartOfDay,
  WeeklyReports,
  MonthlyInvoice,
  Autosave,

  // Async
  FadeTransition,
  AnnualCompetition,
  MonthlyCompetition,
};

struct TurnPipelineContext {
  // Player whose turn just ended, used by things like WeeklyReports and
  // MonthlyInvoice
  struct PreviousPlayer {
    core::PlayerId id;
    std::vector<core::RestaurantId> restaurant_ids;
  } previous;

  // Player whose turn is starting, used for things starting on the next day
  struct NextPlayer {
    core::time::TimeOfDay start_time;
  } next;

  // snapshot of conditions:
  bool is_wrapped{false};
  bool is_end_of_week{false};
  bool is_end_of_month{false};
  bool is_end_of_year{false};

  int fade_frames{20};
};
