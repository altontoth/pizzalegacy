/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "core/common/Ids.h"

class GameState;
class Player;

// Names a pizza in its owner's own id space: a human's recipe, an AI's line in
// PizzaCharts::ai_pizza_names(). The owner decides which of the two it is.
struct PizzaChartRef {
  // A pizza whose owner no longer has it, which therefore matches nothing
  // (can happen with legacy saves).
  static constexpr int unresolved = -1;

  int value{unresolved};

  static constexpr PizzaChartRef of(core::RecipeId id) { return {id.value}; }
  static constexpr PizzaChartRef of(core::AiPizzaId id) { return {id.value}; }

  friend constexpr bool operator==(PizzaChartRef, PizzaChartRef) = default;
};

struct PizzaChartEntry {
  uint8_t previous_position{0}; // 0 = new entry ("-")
  core::PlayerId player_id{0};
  PizzaChartRef pizza_ref{};
  int score{0};
};

class PizzaCharts {
public:
  // The chart is a top {places}:
  static constexpr int places = 10;

  void build(GameState& game_state);

  // The canonical AI pizza names, MARKT.E lines 51 to 149.
  static const std::vector<std::string>& ai_pizza_names();

  // Resolves a reference against the owner it belongs to.
  static std::string pizza_name(const Player& owner, PizzaChartRef pizza);
  [[nodiscard]] const std::vector<PizzaChartEntry>& entries() const {
    return entries_;
  }

  // For save/load only:
  void set_entries(std::vector<PizzaChartEntry> e) { entries_ = std::move(e); }

private:
  std::vector<PizzaChartEntry> entries_;
};
