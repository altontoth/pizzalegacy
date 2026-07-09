/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/mission/MissionTarget.h"
#include "core/time/GameDate.h"

#include <vector>

namespace core::mission {

/**
 * Mission scoring, matching the original game exactly.
 *
 * Each target milestone is matched by event id against the live CV and
 * scored by date closeness; the overall mission score is the average over
 * all target milestones (unachieved ones count as 0).
 */

// Original quirk: a year weighs 45 date units, not 360 (12 months * 30
// days). A full year of error costs less than two months. Reproduced
// exactly for identical scores.
constexpr int DATE_YEAR_WEIGHT = 45;
constexpr int DATE_MONTH_WEIGHT = 30;
constexpr int MILESTONE_MAX_SCORE = 100;

// Date collapsed to the original's scoring units. Any consistent year
// base works since only differences are used.
int date_ordinal(const GameDate& d);

int date_distance(const GameDate& a, const GameDate& b);

// clamp(100 - distance, 0, 100): exact date = 100, more than ~100 units
// off = 0.
int milestone_score(const GameDate& actual, const GameDate& target);

// One score per target milestone, aligned with target.milestones.
// Milestones whose event never occurs in the live CV score 0.
std::vector<int>
per_milestone_scores(const MissionTarget& target,
                     const std::vector<TargetMilestone>& live_cv);

// Integer average of per_milestone_scores over all target milestones.
// 0 for an empty target.
int overall_score(const MissionTarget& target,
                  const std::vector<TargetMilestone>& live_cv);

} // namespace core::mission
