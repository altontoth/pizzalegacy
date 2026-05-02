/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include "core/state/CompetitionSession.h"

struct AnnualCompetitionFlow {
  enum class Phase {
    Inactive,
    Announcing,     // Mayor PizzaOfTheYearCompetition state
    AwaitingDesign, // PizzaDesign AnnualCompetitionDesign state
    ShowingWinner,  // Mayor PizzaOfTheYearWinner state
  };

  Phase phase{Phase::Inactive};
  std::shared_ptr<CompetitionSession> session;
};
