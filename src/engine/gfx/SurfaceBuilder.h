/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "engine/gfx/Draw.h"
#include "engine/gfx/Surface.h"

namespace gfx {

class SurfaceBuilder {
public:
  static SurfaceBuilder create(int width, int height,
                               PaletteHandle palette = {0},
                               uint8_t fill_idx = 0);

  /* Blit source onto our surface (masked or raw) */
  SurfaceBuilder& blit(const Surface& src, ViewPoint dst, bool masked = true,
                       uint8_t transparent = TRANSPARENT);

  SurfaceBuilder& box(int x1, int y1, int x2, int y2, uint8_t color,
                      int thickness = 1);

  SurfaceBuilder& line(ViewPoint p1, ViewPoint p2, uint8_t color,
                       int thickness = 1);

  SurfaceBuilder& line(int x1, int y1, int x2, int y2, uint8_t color,
                       int thickness = 1);

  SurfaceBuilder& fill(uint8_t color);
  SurfaceBuilder& fill_rect(ViewRect r, uint8_t color);
  SurfaceBuilder& put(ViewPoint p, uint8_t idx);

  Surface build() { return std::move(surface_); }

private:
  explicit SurfaceBuilder(Surface s) : surface_(std::move(s)) {}

  Surface surface_;
};

} // namespace gfx
