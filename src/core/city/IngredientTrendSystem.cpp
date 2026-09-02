/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/IngredientTrendSystem.h"

#include "engine/Random.h"

#include <cstdint>
#include <cstdlib>

namespace {

// The original stores trends in a signed byte, so stepping past the target
// can wrap around.
int wrapped(int value) { return static_cast<int8_t>(value); }

int signed_step(int target) { return target < 0 ? -4 : 4; }

} // namespace

void IngredientTrendSystem::daily_tick() {
  for (int i = 0; i < max_ingredient_count; ++i) {
    if (duration_[i] == TrendDuration::None) {
      continue;
    }
    // The original advances each ingredient with a 50% chance per day.
    if (!engine::random::coin_flip()) {
      continue;
    }

    if (duration_[i] == TrendDuration::Countdown) {
      tick_countdown(i);
    }
    else if (approaching_[i]) {
      tick_approaching(i);
    }
    else {
      tick_decaying(i);
    }
  }
}

void IngredientTrendSystem::tick_approaching(int i) {
  const int target = targets_[i];
  // A target of zero leaves the slot alone: it neither moves nor clears.
  if (target == 0) {
    return;
  }

  if (duration_[i] == TrendDuration::Exponential) {
    // Arithmetic shift, so it rounds towards negative infinity.
    current_[i] = wrapped(current_[i] + ((target - current_[i]) >> 1));
  }
  else {
    current_[i] = wrapped(current_[i] + signed_step(target));
  }

  // The tolerance is |target|/10 truncated, so a target under ten has none at
  // all and the trend steps straight past.
  if (std::abs(target - current_[i]) <= std::abs(target / 10)) {
    approaching_[i] = false;
  }
}

void IngredientTrendSystem::tick_decaying(int i) {
  const int target = targets_[i];
  if (target == 0) {
    return;
  }

  current_[i] = wrapped(current_[i] - signed_step(target));
  if (std::abs(current_[i]) < 4) {
    clear_slot(i);
  }
}

void IngredientTrendSystem::tick_countdown(int i) {
  targets_[i]--;
  if (targets_[i] == 0) {
    clear_slot(i);
  }
}

void IngredientTrendSystem::weekly_seed() {
  for (int i = 0; i < max_ingredient_count; ++i) {
    if (!core::food::is_valid_ingredient_id(static_cast<uint8_t>(i))) {
      continue;
    }
    if (!engine::random::rand_percentage_chance(3)) {
      continue;
    }
    apply_effect(static_cast<core::food::IngredientType>(i),
                 {.initial_value = 0,
                  .control = engine::random::rand_max_n(60) - 30,
                  .duration = TrendDuration::Linear});
  }
}
