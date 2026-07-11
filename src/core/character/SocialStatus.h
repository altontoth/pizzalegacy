/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <array>

struct RankChange {
  int old_rank = 0;
  int new_rank = 0;
  bool promoted() const { return new_rank > old_rank; }
  bool demoted() const { return new_rank < old_rank; }
  bool changed() const { return new_rank != old_rank; }
};

class SocialStatus {
public:
  static constexpr int min_score = 0;
  static constexpr int max_score = 65535;

  int social_score() const { return social_score_; }
  void social_score(int v) { social_score_ = v; }
  int underworld_score() const { return underworld_score_; }
  void underworld_score(int v) { underworld_score_ = v; }

  RankChange adjust_underworld(int delta, int difficulty);
  RankChange adjust_social(int delta, int difficulty);

private:
  int social_score_ = 0;
  int underworld_score_ = 0;
};

// Rank lookup table
// Used for both underworld and social (businessworld) scores. The last score is
// not normally useable since the rank method cuts off at 12.
inline constexpr std::array<int, 14> social_score_table = {
    100,   300,    500,    900,    1'500,  2'500,  4'000,
    7'000, 10'000, 15'000, 20'000, 30'000, 40'000, 50'000,
};

// Returns the difficulty-scaled threshold for a given rank index (0-13).
inline int scaled_threshold(int rank_index, int difficulty) {
  int base = social_score_table[rank_index];
  if (rank_index == 0) {
    return base;
  }
  if (rank_index == 1) {
    return base + (difficulty - 3) * 50;
  }
  if (rank_index == 2) {
    return base + (difficulty - 3) * 100;
  }
  return std::min(base * (difficulty + 1) / 4, 65000);
}

inline int social_rank(int score, int difficulty) {
  int rank = 0;
  for (int r = 0; r < static_cast<int>(social_score_table.size()); r++) {
    if (score < scaled_threshold(r, difficulty)) {
      break;
    }
    rank = r + 1;
  }
  return std::min(rank, 12);
}

inline RankChange SocialStatus::adjust_underworld(int delta, int difficulty) {
  int old_r = social_rank(underworld_score_, difficulty);
  underworld_score_ =
      std::clamp(underworld_score_ + delta, min_score, max_score);
  int new_r = social_rank(underworld_score_, difficulty);
  return {.old_rank = old_r, .new_rank = new_r};
}

inline RankChange SocialStatus::adjust_social(int delta, int difficulty) {
  int old_r = social_rank(social_score_, difficulty);
  social_score_ = std::clamp(social_score_ + delta, min_score, max_score);
  int new_r = social_rank(social_score_, difficulty);
  return {.old_rank = old_r, .new_rank = new_r};
}

// Progress from the current rank's threshold to the next.
inline double social_rank_progress(int score, int rank, int difficulty) {
  constexpr int max_rank = 12;
  if (rank >= max_rank) {
    return 1.0;
  }
  int prev = (rank == 0) ? 0 : scaled_threshold(rank - 1, difficulty);
  int next = scaled_threshold(rank, difficulty);
  int range = next - prev;
  if (range <= 0) {
    return 1.0;
  }
  return std::clamp(static_cast<double>(score - prev) / range, 0.0, 1.0);
}
