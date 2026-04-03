/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <optional>

#include "core/common/Ids.h"
#include "core/util/Constants.h"

class GameState;
class City;

inline constexpr int CONTROL_MAP_CELLS =
    NUMBER_OF_DISTRICT_CELL_COLUMNS * NUMBER_OF_DISTRICT_CELL_ROWS;
inline constexpr int TILES_PER_CELL = 8; // each cell covers 8x8 map tiles

// Computes and stores territory control for a city's 20x15 grid.
// Each cell is won by the player whose restaurant exerts the strongest
// influence at that location, or falls back to district ownership.
class ControlMap {
public:
  void build(const GameState& game_state, const City& city);

  std::optional<core::PlayerId> cell_owner(int cell_index) const {
    return cell_owners_[cell_index];
  }

  float player_percentage(int player_index) const {
    return player_percentages_[player_index];
  }

  // Returns the player with the highest market share, or nullopt if nobody
  // controls any cells.
  std::optional<core::PlayerId> market_leader() const;

  // For save/load
  const std::array<std::optional<core::PlayerId>, CONTROL_MAP_CELLS>&
  cell_owners() const {
    return cell_owners_;
  }
  const std::array<float, NUMBER_OF_PLAYERS>& player_percentages() const {
    return player_percentages_;
  }
  void set_cell_owners(
      std::array<std::optional<core::PlayerId>, CONTROL_MAP_CELLS> owners) {
    cell_owners_ = owners;
  }
  void set_player_percentages(std::array<float, NUMBER_OF_PLAYERS> pcts) {
    player_percentages_ = pcts;
  }

private:
  std::array<std::optional<core::PlayerId>, CONTROL_MAP_CELLS> cell_owners_{};
  std::array<float, NUMBER_OF_PLAYERS> player_percentages_{};
};
