/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/EconomyTrend.h"
#include "core/state/GameOptions.h"
#include "core/util/ConfigManager.h"
#include "engine/Random.h"

#include <algorithm>

void EconomyTrend::tick() {
  if (counter_ == 0) {
    start_new_trend();
  }

  const int direction = (target_ - current_ >= 0) ? 1 : -1;
  current_ += direction * engine::random::rand_max_n(5);

  // Append to history (sliding window of last N weeks)
  if (history_count_ < max_history) {
    history_[history_count_] = current_;
    ++history_count_;
  }
  else {
    // Yes I'm aware this is O(N) and there are more efficient ways to do it,
    // but this runs once per game-week so I prefer the readability here:
    std::ranges::rotate(history_, history_.begin() + 1);
    // Oldest entry is now at the end, overwrite it:
    history_.back() = current_;
  }

  // A byte in the original, so a fresh count of 0 wraps to 255 here and holds
  // the trend for 256 weeks.
  --counter_;
}

void EconomyTrend::start_new_trend() {
  target_ = engine::random::rand_max_n(240) - 120;
  // Shifted to 1 to 40 weeks when fixing bugs, which keeps the decrement in
  // tick() away from wrapping around.
  const bool fix = ConfigManager::instance()->options().fix_legacy_bugs;
  counter_ =
      static_cast<uint8_t>(engine::random::rand_max_n(40) + (fix ? 1 : 0));
}
