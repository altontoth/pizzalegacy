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
  uint8_t previous_position{0}; // 0 = new entry ("-")
  core::PlayerId player_id{0};
  std::string pizza_name;
  std::string player_name;
  int score{0};
};

// Global daily top-10 pizza ranking across all players and cities, matching the
// original's single ranking table. Rebuilt from human restaurant sales and AI
// menu slot scores. Each City keeps its own instance so the previous-position
// arrows diff against that city's last rebuild, but the ranked contents are
// identical everywhere.
class PizzaCharts {
public:
  void build(GameState& game_state);

  // The 100 canonical AI pizza names (MARKT.E lines 51-150), indexed 0-based.
  static const std::vector<std::string>& ai_pizza_names();
  [[nodiscard]] const std::vector<PizzaChartEntry>& entries() const {
    return entries_;
  }
  [[nodiscard]] const std::vector<PizzaChartEntry>& previous() const {
    return previous_;
  }

  // For save/load only:
  void set_entries(std::vector<PizzaChartEntry> e) { entries_ = std::move(e); }
  void set_previous(std::vector<PizzaChartEntry> p) {
    previous_ = std::move(p);
  }

private:
  std::vector<PizzaChartEntry> entries_;
  std::vector<PizzaChartEntry> previous_;
};
