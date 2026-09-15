/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <span>
#include <vector>

#include "engine/Size.h"
#include "engine/geom/Spaces.h"
#include "views/screens/restaurant/Walk.h"

// Grid-based pathfinding for restaurant walkers (guests and waiters). Pure
// geometry: callers supply room dimensions and obstacle rectangles in world
// coordinates; paths come back as Orientation steps of WALK_CELL_SIZE each.
namespace views::screens::restaurant {

struct NavGrid {
  std::vector<bool> blocked;
  int w{0};
  int h{0};
};

struct WalkingPath {
  std::vector<Orientation> steps;
  // True when the destination was reached. steps is empty both for an
  // unreachable destination and for one the walker already stands on.
  bool reachable{false};
  // Grid snapshot the path was computed on, kept for the walk debug overlay.
  std::vector<bool> blocked;
  int grid_w{0};
  int grid_h{0};
};

// Rasterize the restaurant onto the walker grid: walls and furniture
// blocked, customer entrance carved through the bottom wall. room_size is the
// interior size in tiles; obstacles are furniture rects in world coordinates.
[[nodiscard]] NavGrid
build_navigation_grid(Size room_size, int entrance_offset,
                      std::span<const WorldRect> obstacles);

// Find a path between two world points on the grid. The passable arg tells us
// which part we can pass through regardless so the route can start or end
// inside a chair.
[[nodiscard]] WalkingPath compute_walking_path(const NavGrid& grid,
                                               WorldPoint from, WorldPoint to,
                                               WorldRect passable);

} // namespace views::screens::restaurant
