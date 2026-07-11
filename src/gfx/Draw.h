/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "gfx/ColorIndex.h"
#include "gfx/Surface.h"

using gfx::Surface;

namespace gfx::draw {
void line(Surface& dst, ViewPoint from, ViewPoint to, ColorIndex color_idx,
          int thickness = 1);
void box(Surface& dst, ViewPoint from, ViewPoint to, ColorIndex color,
         int thickness = 1);
void box(Surface& dst, ViewRect r, ColorIndex color, int thickness = 1);
void grid(Surface& dst, ViewRect bounds, ColorIndex color_idx,
          int h_spacing = 12, int v_spacing = 14);
void beveled_box(Surface& dst, ViewRect r, ColorIndex light_color,
                 ColorIndex dark_color, int thickness = 2);
void fill_circle(Surface& dst, ViewPoint center, int radius, ColorIndex color);

} // namespace gfx::draw
