/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <array>
#include <numeric>
#include "core/common/Ids.h"
#include "core/food/Recipe.h"

// Number of menu slots an AI player owns.
inline constexpr int ai_menu_slot_count = 10;

// An empty menu slot.
inline constexpr core::AiPizzaId ai_pizza_none{0};

// Used only for Pizza Charts ranking, not restaurant economics.
struct AiMenuSlot {
  core::AiPizzaId pizza_id{ai_pizza_none};
  int production_counter{0};
  int max_production{0};
  std::array<int, core::food::chart_history_days> history{};

  [[nodiscard]] int chart_score() const {
    return std::accumulate(history.begin(), history.end(), 0);
  }

  // Drop the oldest day and store another day's production.
  void update(int restaurant_count) {
    std::ranges::copy(history.begin() + 1, history.end(), history.begin());
    if (pizza_id != ai_pizza_none) {
      history.back() = restaurant_count * production_counter;
    }
  }
};
