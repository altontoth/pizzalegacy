/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "core/common/Ids.h"
#include "core/events/MilestoneId.h"
#include "core/state/AnnualCompetitionFlow.h"
#include "core/state/CompetitionSession.h"

class GameState;

// A player's accepted entry into the monthly pizza competition, collected by
// the UI layer's entry popups.
struct MonthlyCompetitionEntry {
  int bet_dollars{0};
  // Set when this entry also chose the competition speed (first accepter).
  std::optional<int> speed_level;
};

// Owns the monthly and annual pizza competition flows, split out of
// GameState for cohesion; it works on GameState internals by design.
class CompetitionCoordinator {
public:
  explicit CompetitionCoordinator(GameState& state) : gs_(&state) {}

  void run_monthly_competition();
  // Returns false when no competition screen was started, so the pipeline
  // knows not to wait for a resume that will never come.
  bool run_annual_pizza_competition();
  void finalize_annual_pizza_competition(CompetitionSession session);
  // Books each participant's bet as spent; called once when a competition
  // session launches. The pot is paid out or refunded in
  // finalize_competition_results().
  void collect_competition_bets(const CompetitionSession& session);
  // Simulates the AI participants, determines the winner and settles the bets
  // of a monthly/challenge competition session. Returns the event to deliver
  // to a human winner once the results screen is dismissed, if any.
  std::optional<core::events::MilestoneId>
  finalize_competition_results(CompetitionSession& session);

  // GameState::update() hook: advances the annual flow once its mayor
  // screen has closed.
  void tick();

  int competition_count() const { return competition_count_; }
  void competition_count(int count) { competition_count_ = count; }
  void increment_competition_count() {
    competition_count_ = std::min(competition_count_ + 1, 255);
  }

private:
  void
  run_monthly_competition_chain(std::vector<core::PlayerId> remaining,
                                std::shared_ptr<CompetitionSession> session);
  void launch_monthly_competition(std::shared_ptr<CompetitionSession> session);
  void advance_annual_competition();
  [[nodiscard]] bool annual_comp_ready_to_advance() const;

  GameState* gs_;
  AnnualCompetitionFlow annual_comp_;
  int competition_count_{0};
};
