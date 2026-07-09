/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/restaurant/WalkNavigation.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <utility>
#include <vector>

#include "core/util/LegacyConstants.h"

using namespace LegacyConstants;

namespace views::screens::restaurant {

namespace {

constexpr int WALK_MAX_OPS = 300;
constexpr int WALK_MAX_DEPTH = 100;

bool walk_dfs(int cx, int cy, int dest_x, int dest_y, int grid_w, int grid_h,
              const std::vector<bool>& blocked, std::vector<bool>& visited,
              std::vector<Orientation>& path, int& ops_remaining, int depth) {
  if (cx == dest_x && cy == dest_y) {
    return true;
  }
  if (ops_remaining <= 0 || depth >= WALK_MAX_DEPTH) {
    return false;
  }
  --ops_remaining;
  visited[(cy * grid_w) + cx] = true;

  using O = Orientation;

  // Score each direction by the signed displacement toward the destination
  // along that axis. Higher orientation value wins ties
  auto dir_score = [&](O dir) -> int {
    switch (dir) {
    case O::S:
      return dest_y - cy;
    case O::W:
      return cx - dest_x;
    case O::N:
      return cy - dest_y;
    case O::E:
      return dest_x - cx;
    }
    return 0;
  };

  std::array<O, ORIENTATION_COUNT> dirs = {O::S, O::W, O::N, O::E};
  std::ranges::sort(dirs, [&](O a, O b) {
    const int sa = dir_score(a);
    const int sb = dir_score(b);
    if (sa != sb) {
      return sa > sb;
    }
    return a > b;
  });

  for (O dir : dirs) {
    const int nx = cx + walk_dx(dir);
    const int ny = cy + walk_dy(dir);
    if (nx < 0 || nx >= grid_w || ny < 0 || ny >= grid_h) {
      continue;
    }
    const int idx = (ny * grid_w) + nx;
    if (blocked[idx] || visited[idx]) {
      continue;
    }
    path.push_back(dir);
    if (walk_dfs(nx, ny, dest_x, dest_y, grid_w, grid_h, blocked, visited, path,
                 ops_remaining, depth + 1)) {
      return true;
    }
    path.pop_back();
  }
  return false;
}

// Collapse runs into L-shapes; the DFS produces staircases that look bad.
bool seg_clear(int x0, int y0, int x1, int y1, int grid_w, int grid_h,
               const std::vector<bool>& blocked) {
  const int dx = (x1 > x0) ? 1 : (x1 < x0) ? -1 : 0;
  const int dy = (y1 > y0) ? 1 : (y1 < y0) ? -1 : 0;
  int x = x0;
  int y = y0;
  while (x != x1 || y != y1) {
    if (x < 0 || x >= grid_w || y < 0 || y >= grid_h ||
        blocked[(y * grid_w) + x]) {
      return false;
    }
    x += dx;
    y += dy;
  }
  return x >= 0 && x < grid_w && y >= 0 && y < grid_h &&
         !blocked[(y * grid_w) + x];
}

std::vector<Orientation> straighten_path(int from_x, int from_y,
                                         const std::vector<Orientation>& path,
                                         int grid_w, int grid_h,
                                         const std::vector<bool>& blocked) {
  using O = Orientation;

  if (path.size() <= 1) {
    return path;
  }

  std::vector<std::pair<int, int>> pos;
  pos.reserve(path.size() + 1);
  pos.emplace_back(from_x, from_y);
  for (auto d : path) {
    pos.emplace_back(pos.back().first + walk_dx(d),
                     pos.back().second + walk_dy(d));
  }

  std::vector<O> result;
  result.reserve(path.size());
  int from_idx = 0;

  while (from_idx < static_cast<int>(path.size())) {
    const auto [fx, fy] = pos[from_idx];
    bool jumped = false;

    // Scan backwards: find the furthest pos[to] reachable via an L-path.
    for (int to = static_cast<int>(pos.size()) - 1; to > from_idx + 1; --to) {
      const auto [tx, ty] = pos[to];

      if (seg_clear(fx, fy, tx, fy, grid_w, grid_h, blocked) &&
          seg_clear(tx, fy, tx, ty, grid_w, grid_h, blocked)) {
        if (tx != fx) {
          const O h_dir = (tx > fx) ? O::E : O::W;
          for (int k = 0; k < std::abs(tx - fx); ++k) {
            result.push_back(h_dir);
          }
        }
        if (ty != fy) {
          const O v_dir = (ty > fy) ? O::S : O::N;
          for (int k = 0; k < std::abs(ty - fy); ++k) {
            result.push_back(v_dir);
          }
        }
        from_idx = to;
        jumped = true;
        break;
      }

      if (seg_clear(fx, fy, fx, ty, grid_w, grid_h, blocked) &&
          seg_clear(fx, ty, tx, ty, grid_w, grid_h, blocked)) {
        if (ty != fy) {
          const O v_dir = (ty > fy) ? O::S : O::N;
          for (int k = 0; k < std::abs(ty - fy); ++k) {
            result.push_back(v_dir);
          }
        }
        if (tx != fx) {
          const O h_dir = (tx > fx) ? O::E : O::W;
          for (int k = 0; k < std::abs(tx - fx); ++k) {
            result.push_back(h_dir);
          }
        }
        from_idx = to;
        jumped = true;
        break;
      }
    }

    if (!jumped) {
      result.push_back(path[from_idx]);
      ++from_idx;
    }
  }

  return result;
}

} // namespace

NavGrid build_navigation_grid(Size room_size, int entrance_offset,
                              std::span<const WorldRect> obstacles) {
  // The navigation grid covers the entire rendered restaurant, including the
  // walls. Furniture coordinates already include this border offset, so world
  // coordinates map directly to grid cells.
  const int grid_w =
      ((room_size.w + 2) * RESTAURANT_TILE_WIDTH + WALK_CELL_SIZE - 1) /
      WALK_CELL_SIZE;
  const int grid_h =
      ((room_size.h + 2) * RESTAURANT_TILE_HEIGHT + WALK_CELL_SIZE - 1) /
      WALK_CELL_SIZE;

  std::vector<bool> blocked(static_cast<size_t>(grid_w * grid_h), false);

  // WALK_CELL_SIZE (12) does not divide evenly into the room height measured
  // in pixels (22-pixel tiles). Near the bottom edge, a navigation cell can
  // partially overlap the wall border: treat those cells as blocked if their
  // lower edge extends into the wall, rather than testing only the top left of
  // the cell.
  for (int gy = 0; gy < grid_h; ++gy) {
    for (int gx = 0; gx < grid_w; ++gx) {
      const int wx = gx * WALK_CELL_SIZE;
      const int wy = gy * WALK_CELL_SIZE;
      const bool left_wall = wx < RESTAURANT_TILE_WIDTH;
      const bool right_wall = wx >= (room_size.w + 1) * RESTAURANT_TILE_WIDTH;
      const bool top_wall = wy < RESTAURANT_TILE_HEIGHT;
      const bool bottom_wall = (gy + 1) * WALK_CELL_SIZE >
                               (room_size.h + 1) * RESTAURANT_TILE_HEIGHT;
      if (left_wall || right_wall || top_wall || bottom_wall) {
        blocked[(gy * grid_w) + gx] = true;
      }
    }
  }

  // A cell is blocked only if its center point lies within the furniture
  // bounds. This prevents furniture from claiming neighbouring cells that are
  // touched only by its outer pixels.
  const int half = WALK_CELL_SIZE / 2;
  for (const auto& r : obstacles) {
    const int x0 = (r.x + half - 1) / WALK_CELL_SIZE;
    const int y0 = (r.y + half - 1) / WALK_CELL_SIZE;
    const int x1 = (r.x + r.w - half - 1) / WALK_CELL_SIZE;
    const int y1 = (r.y + r.h - half - 1) / WALK_CELL_SIZE;
    for (int y = std::max(0, y0); y <= std::min(grid_h - 1, y1); ++y) {
      for (int x = std::max(0, x0); x <= std::min(grid_w - 1, x1); ++x) {
        blocked[(y * grid_w) + x] = true;
      }
    }
  }

  // Carve the customer entrance through the bottom wall so characters can walk
  // in and out through the doorway. The opening is 3 tiles wide at the entrance
  // offset (matching the brown-tile gap drawn by render_side_and_bottom_walls).
  // Done last so it overrides both the wall and any furniture in the doorway.
  const int ent_wx0 = entrance_offset * RESTAURANT_TILE_WIDTH;
  const int ent_wx1 =
      (entrance_offset + 3) * RESTAURANT_TILE_WIDTH; // exclusive
  for (int gy = 0; gy < grid_h; ++gy) {
    const bool bottom_wall =
        (gy + 1) * WALK_CELL_SIZE > (room_size.h + 1) * RESTAURANT_TILE_HEIGHT;
    if (!bottom_wall) {
      continue;
    }
    for (int gx = 0; gx < grid_w; ++gx) {
      const int wx = gx * WALK_CELL_SIZE;
      // Unblock any cell that overlaps the doorway opening.
      if (wx + WALK_CELL_SIZE > ent_wx0 && wx < ent_wx1) {
        blocked[(gy * grid_w) + gx] = false;
      }
    }
  }

  return {.blocked = std::move(blocked), .w = grid_w, .h = grid_h};
}

WalkingPath compute_walking_path(const NavGrid& grid, WorldPoint from,
                                 WorldPoint to, WorldPoint dest_origin,
                                 Size dest_size) {
  // Copy the grid so we can mutate it (unblock the destination chair).
  std::vector<bool> blocked = grid.blocked;
  const int grid_w = grid.w;
  const int grid_h = grid.h;

  // Clamp to grid bounds
  const auto clamp_cell = [&](int v, int hi) {
    return std::clamp(v, 0, hi - 1);
  };

  // Unblock the destination chair so DFS can enter from any exposed edge
  const int dest_gx = clamp_cell(to.x / WALK_CELL_SIZE, grid_w);
  const int dest_gy = clamp_cell(to.y / WALK_CELL_SIZE, grid_h);
  {
    const int ux0 = dest_origin.x / WALK_CELL_SIZE;
    const int uy0 = dest_origin.y / WALK_CELL_SIZE;
    const int ux1 = (dest_origin.x + dest_size.w - 1) / WALK_CELL_SIZE;
    const int uy1 = (dest_origin.y + dest_size.h - 1) / WALK_CELL_SIZE;
    for (int y = std::max(0, uy0); y <= std::min(grid_h - 1, uy1); ++y) {
      for (int x = std::max(0, ux0); x <= std::min(grid_w - 1, ux1); ++x) {
        blocked[(y * grid_w) + x] = false;
      }
    }
  }

  const int from_gx = clamp_cell(from.x / WALK_CELL_SIZE, grid_w);
  const int from_gy = clamp_cell(from.y / WALK_CELL_SIZE, grid_h);

  std::vector<Orientation> path;
  std::vector<bool> visited(static_cast<size_t>(grid_w * grid_h), false);
  int ops = WALK_MAX_OPS;
  walk_dfs(from_gx, from_gy, dest_gx, dest_gy, grid_w, grid_h, blocked, visited,
           path, ops, 0);
  path = straighten_path(from_gx, from_gy, path, grid_w, grid_h, blocked);
  return {.steps = std::move(path),
          .blocked = std::move(blocked),
          .grid_w = grid_w,
          .grid_h = grid_h};
}

} // namespace views::screens::restaurant
