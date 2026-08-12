/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>

#include "engine/geom/Spaces.h"

// Shared navigation grid for restaurant navigation. The walker grid cell is
// WALK_CELL_SIZE pixels square; directions index the tables in Orientation
// order.
namespace views::screens::restaurant {

enum class Orientation { S = 0, W = 1, N = 2, E = 3 };

// Number of facings in Orientation; also the modulus for turning.
constexpr int ORIENTATION_COUNT = 4;

constexpr int WALK_CELL_SIZE = 12;
constexpr std::array<int, ORIENTATION_COUNT> WALK_DX = {0, -1, 0, 1};
constexpr std::array<int, ORIENTATION_COUNT> WALK_DY = {1, 0, -1, 0};

constexpr int walk_dx(Orientation o) { return WALK_DX[static_cast<int>(o)]; }
constexpr int walk_dy(Orientation o) { return WALK_DY[static_cast<int>(o)]; }

constexpr Orientation opposite(Orientation o) {
  int turned = static_cast<int>(o) + (ORIENTATION_COUNT / 2);
  if (turned >= ORIENTATION_COUNT) {
    turned -= ORIENTATION_COUNT;
  }
  return static_cast<Orientation>(turned);
}

// Advance a walker one grid cell in the given direction and bump its
// animation phase.
inline void step_one_cell(WorldPoint& position, int& walk_cycle,
                          Orientation dir) {
  position.x += walk_dx(dir) * WALK_CELL_SIZE;
  position.y += walk_dy(dir) * WALK_CELL_SIZE;
  walk_cycle = (walk_cycle + 1) % 4;
}

// Maps the 4-phase walk cycle onto the 3 sprite frames per direction:
// neutral, left-step, neutral, right-step.
constexpr std::array<int, 4> WALK_CYCLE_TO_FRAME = {0, 1, 0, 2};

// Frame index into a walk bob for a facing and walk-cycle phase.
constexpr int walk_frame_for(Orientation dir, int walk_cycle) {
  return (static_cast<int>(dir) * 3) + WALK_CYCLE_TO_FRAME[walk_cycle];
}

} // namespace views::screens::restaurant
