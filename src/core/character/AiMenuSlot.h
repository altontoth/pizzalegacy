/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <array>
#include <numeric>
#include "core/food/Recipe.h"

// Each AI player has 10 of these, representing their pizza menu.
// Used only for Pizza Charts ranking, not restaurant economics.
struct AiMenuSlot {
  int recipe_id{0};
  int production_counter{0};
  int max_production{0};
  std::array<int, core::food::chart_history_days> history{};

  [[nodiscard]] int chart_score() const {
    return std::accumulate(history.begin(), history.end(), 0);
  }

  // Drop the oldest day and store another day's production.
  void update(int restaurant_count) {
    std::ranges::copy(history.begin() + 1, history.end(), history.begin());
    if (recipe_id != 0) {
      history.back() = restaurant_count * production_counter;
    }
  }
};
