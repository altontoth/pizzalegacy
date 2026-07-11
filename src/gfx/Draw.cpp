/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gfx/Draw.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "gfx/Surface.h"

namespace gfx::draw {

// Bresenham line
void line(Surface& dst, ViewPoint from, ViewPoint to, ColorIndex color_idx,
          int thickness) {
  thickness = std::max(thickness, 1);

  // Optimize straight lines
  if (from.y == to.y) {
    int x0 = std::min(from.x, to.x);
    int w = std::abs(to.x - from.x) + 1;
    dst.fill_rect({x0, from.y, w, thickness}, color_idx);
    return;
  }
  if (from.x == to.x) {
    int y0 = std::min(from.y, to.y);
    int h = std::abs(to.y - from.y) + 1;
    dst.fill_rect({from.x, y0, thickness, h}, color_idx);
    return;
  }

  // Bresenham setup
  int dx = std::abs(to.x - from.x);
  int sx = (from.x < to.x) ? 1 : -1;
  int dy = -std::abs(to.y - from.y);
  int sy = (from.y < to.y) ? 1 : -1;
  int err = dx + dy;

  auto plot = [&](int px, int py) {
    // draw a solid square thickness x thickness at (px,py)
    for (int ty = 0; ty < thickness; ++ty) {
      for (int tx = 0; tx < thickness; ++tx) {
        dst.put({px + tx, py + ty}, color_idx);
      }
    }
  };

  while (true) {
    plot(from.x, from.y);

    if (from.x == to.x && from.y == to.y) {
      break;
    }

    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      from.x += sx;
    }
    if (e2 <= dx) {
      err += dx;
      from.y += sy;
    }
  }
}

// Hollow rectangle (box) using the line function
void box(Surface& dst, ViewPoint from, ViewPoint to, ColorIndex color,
         int thickness) {
  // Normalise coordinates so from.x <= to.x, from.y <= to.y
  if (from.x > to.x) {
    std::swap(from.x, to.x);
  }
  if (from.y > to.y) {
    std::swap(from.y, to.y);
  }

  draw::line(dst, {from.x, from.y}, {to.x, from.y}, color, thickness); // top
  draw::line(dst, {to.x, from.y}, {to.x, to.y}, color, thickness);     // right
  draw::line(dst, {to.x, to.y}, {from.x, to.y}, color, thickness);     // bottom
  draw::line(dst, {from.x, to.y}, {from.x, from.y}, color, thickness); // left
}

// Hollow rectangle (box) using the line function
void box(Surface& dst, ViewRect r, ColorIndex color, int thickness) {
  box(dst, {r.x, r.y}, {r.x + r.w - 1, r.y + r.h - 1}, color, thickness);
}

void grid(Surface& dst, ViewRect bounds, ColorIndex color_idx, int h_spacing,
          int v_spacing) {
  // Draw vertical lines
  for (int x = bounds.x; x < bounds.x + bounds.w; x += h_spacing) {
    gfx::draw::line(dst, ViewPoint{x, bounds.y},
                    ViewPoint{x, bounds.y + bounds.h - 1}, color_idx, 2);
  }

  // Draw horizontal lines
  for (int y = bounds.y; y < bounds.y + bounds.h; y += v_spacing) {
    gfx::draw::line(dst, ViewPoint{bounds.x, y},
                    ViewPoint{bounds.x + bounds.w - 1, y}, color_idx, 2);
  }
}

// Technically the game has two types of beveled boxes, one that's asymmetrical
// and one that's symmetrical, but I can't be bothered. Also we're rendering
// too detailed instead of doing proper 2x of the original game but I also
// am fine with that.
void beveled_box(Surface& dst, ViewRect r, ColorIndex light_color,
                 ColorIndex dark_color, int thickness) {

  // Precompute corners
  ViewPoint top_left{r.x, r.y};
  ViewPoint top_right{r.x + r.w - 1, r.y};
  ViewPoint bottom_left{r.x, r.y + r.h - 1};
  ViewPoint bottom_right{r.x + r.w - 1, r.y + r.h - 1};

  // Top + Left (light color)
  for (int i = 0; i < thickness; ++i) {
    line(dst, {top_left.x + i, top_left.y + i},
         {top_right.x - i, top_right.y + i}, light_color);
    line(dst, {top_left.x + i, top_left.y + i},
         {bottom_left.x + i, bottom_left.y - i}, light_color);
  }

  // Bottom + Right (dark color)
  for (int i = 0; i < thickness; ++i) {
    line(dst, {bottom_left.x + i, bottom_left.y - i},
         {bottom_right.x - i, bottom_right.y - i}, dark_color);
    line(dst, {top_right.x - i, top_right.y + i},
         {bottom_right.x - i, bottom_right.y - i}, dark_color);
  }
}

// Solid disc, filled scanline by scanline from the circle equation.
void fill_circle(Surface& dst, ViewPoint center, int radius, ColorIndex color) {
  if (radius <= 0) {
    return;
  }
  for (int dy = -radius; dy <= radius; ++dy) {
    int dx = static_cast<int>(std::lround(
        std::sqrt((static_cast<double>(radius) * radius) - (dy * dy))));
    line(dst, {center.x - dx, center.y + dy}, {center.x + dx, center.y + dy},
         color);
  }
}
} // namespace gfx::draw
