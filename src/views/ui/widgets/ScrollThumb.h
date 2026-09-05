/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include "engine/Size.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/Surface.h"

// Draws the scroll bar thumb on the right edge of a list.
inline void draw_scroll_thumb(gfx::Surface& block, Size size, int scroll_offset,
                              int visible, int total) {
  if (visible >= total || total == 0) {
    return;
  }

  constexpr uint8_t thumb_color = 14;
  constexpr int thumb_w = 4;
  constexpr int thumb_margin = 1;

  const int max_scroll = total - visible;
  const int track_h = size.h;
  const int thumb_h = std::max(10, visible * track_h / total);
  const int scroll = std::min(scroll_offset, max_scroll);
  const int thumb_y = scroll * (track_h - thumb_h) / max_scroll;

  ViewRect thumb{size.w - thumb_w - thumb_margin, thumb_y, thumb_w, thumb_h};
  block.fill_rect(thumb, thumb_color);
}
