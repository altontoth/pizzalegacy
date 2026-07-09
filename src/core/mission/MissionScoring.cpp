/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/mission/MissionScoring.h"

#include <algorithm>
#include <cstdlib>

namespace core::mission {

int date_ordinal(const GameDate& d) {
  return (d.year_short() * DATE_YEAR_WEIGHT) +
         ((static_cast<int>(d.month()) - 1) * DATE_MONTH_WEIGHT) +
         (static_cast<int>(d.day()) - 1);
}

int date_distance(const GameDate& a, const GameDate& b) {
  return std::abs(date_ordinal(a) - date_ordinal(b));
}

int milestone_score(const GameDate& actual, const GameDate& target) {
  return std::clamp(MILESTONE_MAX_SCORE - date_distance(actual, target), 0,
                    MILESTONE_MAX_SCORE);
}

std::vector<int>
per_milestone_scores(const MissionTarget& target,
                     const std::vector<TargetMilestone>& live_cv) {
  std::vector<int> scores;
  scores.reserve(target.milestones.size());
  for (const auto& milestone : target.milestones) {
    int score = 0;
    // The live CV is fire-once per event id, so at most one entry matches.
    for (const auto& achieved : live_cv) {
      if (achieved.message_id == milestone.message_id) {
        score = milestone_score(achieved.date, milestone.date);
        break;
      }
    }
    scores.push_back(score);
  }
  return scores;
}

int overall_score(const MissionTarget& target,
                  const std::vector<TargetMilestone>& live_cv) {
  if (target.milestones.empty()) {
    return 0;
  }
  const auto scores = per_milestone_scores(target, live_cv);
  int total = 0;
  for (int s : scores) {
    total += s;
  }
  // Integer division like the original.
  return total / static_cast<int>(scores.size());
}

} // namespace core::mission
