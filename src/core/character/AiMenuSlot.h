/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <array>
#include <numeric>

// Each AI player has 10 of these, representing their pizza menu.
// Used only for Pizza Charts ranking, not restaurant economics.
struct AiMenuSlot {
  static constexpr int HISTORY_SIZE = 7;

  int recipe_id{0};
  int production_counter{0};
  int max_production{0};
  std::array<int, HISTORY_SIZE> history{};

  [[nodiscard]] int chart_score() const {
    return std::accumulate(history.begin(), history.end(), 0);
  }

  void update(int restaurant_count) {
    std::ranges::rotate(history, history.begin() + 1);
    history.back() = restaurant_count * production_counter;
  }
};
