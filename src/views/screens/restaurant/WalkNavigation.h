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
  // Grid snapshot the path was computed on (with the destination unblocked),
  // kept for the walk debug overlay.
  std::vector<bool> blocked;
  int grid_w{0};
  int grid_h{0};
};

// Rasterize the dining room onto the walker grid: walls and furniture
// blocked, customer entrance carved through the bottom wall. room_size is the
// interior size in tiles; obstacles are furniture rects in world coordinates.
[[nodiscard]] NavGrid
build_navigation_grid(Size room_size, int entrance_offset,
                      std::span<const WorldRect> obstacles);

// Find a path between two world points on the grid. The destination furniture
// rect (dest_origin/dest_size) is unblocked so the path may end inside it.
// steps is empty when no path was found.
[[nodiscard]] WalkingPath compute_walking_path(const NavGrid& grid,
                                               WorldPoint from, WorldPoint to,
                                               WorldPoint dest_origin,
                                               Size dest_size);

} // namespace views::screens::restaurant
