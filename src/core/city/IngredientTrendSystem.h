/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include "core/food/Ingredient.h"

// How a trend approaches its target value over time.
enum class TrendDuration {
  None = 0,
  Linear = 1,      // +/- 4 per day toward target, then +/- 4 decay back to 0
  Exponential = 2, // Halve distance each day, then +/1 4 decay back to 0
  Countdown = 3,   // Decrement target each day, clear slot when it hits 0
  Count = 4,
};

struct TrendEffect {
  // Starting trend value (written to current)
  int initial_value;
  // Target value (Linear/Exponential) or number of days (Countdown)
  int control;
  // How the trend progresses over time
  TrendDuration duration;
};

constexpr std::string_view to_string(TrendDuration d) {
  switch (d) {
  case TrendDuration::None:
    return "none";
  case TrendDuration::Linear:
    return "linear";
  case TrendDuration::Exponential:
    return "exponential";
  case TrendDuration::Countdown:
    return "countdown";
  case TrendDuration::Count:
    return "count";
  }
  return "unknown";
}

using core::food::IngredientArray;
using core::food::IngredientTrends;

// Manages the ingredient trend slots
// Each slot tracks one ingredient's current popularity trend value,
// a target it's moving toward, and a duration type controlling the approach.
class IngredientTrendSystem {
public:
  void apply_effect(core::food::IngredientType type,
                    const TrendEffect& effect) {
    current_[type] = effect.initial_value;
    targets_[type] = effect.control;
    duration_[type] = effect.duration;
    approaching_[type] = true;
  }

  void daily_tick() {
    for (int i = 0; i < max_ingredient_count; ++i) {
      if (duration_[i] == TrendDuration::None) {
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

  [[nodiscard]] int get_trend(core::food::IngredientType type) const {
    return current_[type];
  }

  [[nodiscard]] const IngredientTrends& trends() const { return current_; }

  void clear() {
    current_.fill(0);
    targets_.fill(0);
    duration_.fill(TrendDuration::None);
    approaching_.fill(false);
  }

  [[nodiscard]] const IngredientTrends& targets() const { return targets_; }
  [[nodiscard]] const IngredientArray<TrendDuration>& durations() const {
    return duration_;
  }
  [[nodiscard]] const IngredientArray<bool>& approaching_flags() const {
    return approaching_;
  }

  void reset_slot(core::food::IngredientType type) {
    clear_slot(static_cast<int>(type));
  }

  void reset_approaching(core::food::IngredientType type) {
    approaching_[type] = false;
  }

private:
  void clear_slot(int i) {
    current_[i] = 0;
    targets_[i] = 0;
    duration_[i] = TrendDuration::None;
    approaching_[i] = false;
  }

  bool reached_target(int i) const {
    int8_t target = targets_[i];
    return std::abs(current_[i] - target) < std::abs(target / 10) + 1;
  }

  // Moving toward target
  void tick_approaching(int i) {
    switch (duration_[i]) {
    case TrendDuration::Linear: {
      int8_t target = targets_[i];
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

  // Decaying back toward 0 (types Linear/Exponential after reaching target)
  void tick_decaying(int i) {
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

  IngredientTrends current_;
  IngredientTrends targets_;
  IngredientArray<TrendDuration> duration_;
  IngredientArray<bool> approaching_;
};
