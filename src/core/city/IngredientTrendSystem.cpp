/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/IngredientTrendSystem.h"

#include "engine/Random.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>

void IngredientTrendSystem::daily_tick() {
  for (int i = 0; i < max_ingredient_count; ++i) {
    if (duration_[i] == TrendDuration::None) {
      continue;
    }
    // The original advances each ingredient with a 50% chance per day.
    if (!engine::random::coin_flip()) {
      continue;
    }

    if (approaching_[i]) {
      tick_approaching(i);
    }
    else {
      tick_decaying(i);
    }
  }
}

bool IngredientTrendSystem::reached_target(int i) const {
  int8_t target = static_cast<int8_t>(targets_[i]);
  return std::abs(current_[i] - target) < std::abs(target / 10) + 1;
}

void IngredientTrendSystem::tick_approaching(int i) {
  switch (duration_[i]) {
  case TrendDuration::Linear: {
    int8_t target = static_cast<int8_t>(targets_[i]);
    if (target < 0) {
      current_[i] -= 4;
    }
    else if (target > 0) {
      current_[i] += 4;
    }
    if (reached_target(i)) {
      current_[i] = targets_[i];
      approaching_[i] = false;
    }
    break;
  }
  case TrendDuration::Exponential: {
    auto half = (targets_[i] - current_[i]) / 2;
    current_[i] = current_[i] + half;
    if (reached_target(i)) {
      current_[i] = targets_[i];
      approaching_[i] = false;
    }
    break;
  }
  case TrendDuration::Countdown: {
    if (targets_[i] > 0) {
      targets_[i]--;
    }
    else {
      targets_[i]++;
    }
    if (targets_[i] == 0) {
      clear_slot(i);
    }
    break;
  }
  default:
    break;
  }
}

void IngredientTrendSystem::tick_decaying(int i) {
  auto val = current_[i];
  if (val > 0) {
    val = std::max(val - 4, 0);
  }
  else if (val < 0) {
    val = std::min(val + 4, 0);
  }
  current_[i] = val;

  if (val == 0) {
    clear_slot(i);
  }
}
