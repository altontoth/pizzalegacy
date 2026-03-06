/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/EconomyTrend.h"
#include "engine/Random.h"

#include <algorithm>

void EconomyTrend::daily_tick() {
  if (days_remaining_ <= 0) {
    pick_new_target();
  }

  int direction = (current_ < target_) ? 1 : (current_ > target_) ? -1 : 0;
  current_ += direction * engine::random::rand_max_n(4);

  // Append to history (sliding window of last N days)
  if (history_count_ < max_history) {
    history_[history_count_] = current_;
    ++history_count_;
  }
  else {
    // Yes I'm aware this is O(N) and there are more efficient ways to do it,
    // but this runs once per game-day so I prefer the readability here:
    std::ranges::rotate(history_, history_.begin() + 1);
    // Oldest entry is now at the end, overwrite it:
    history_.back() = current_;
  }

  days_remaining_--;
}

void EconomyTrend::pick_new_target() {
  target_ = engine::random::rand_between(-120, 120);
  days_remaining_ = engine::random::rand_max_n(39);
}
