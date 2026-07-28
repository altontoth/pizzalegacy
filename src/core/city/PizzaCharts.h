/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>
#include "core/common/Ids.h"

class GameState;

struct PizzaChartEntry {
  // A pizza whose owner no longer has it, which therefore matches nothing
  // (can happen with legacy saves).
  static constexpr int unresolved_ref = -1;

  uint8_t previous_position{0}; // 0 = new entry ("-")
  core::PlayerId player_id{0};
  // Identifies the pizza: a recipe id for a human, a menu slot id for an AI.
  int pizza_ref{unresolved_ref};
  std::string pizza_name;
  std::string player_name;
  int score{0};
};

class PizzaCharts {
public:
  // The chart is a top {places}:
  static constexpr int places = 10;

  void build(GameState& game_state);

  // The 100 canonical AI pizza names (MARKT.E lines 51-150), indexed 0-based.
  static const std::vector<std::string>& ai_pizza_names();
  [[nodiscard]] const std::vector<PizzaChartEntry>& entries() const {
    return entries_;
  }

  // For save/load only:
  void set_entries(std::vector<PizzaChartEntry> e) { entries_ = std::move(e); }

private:
  std::vector<PizzaChartEntry> entries_;
};
