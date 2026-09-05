/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/gfx/SurfaceBuilder.h"
#include "engine/Logger.h"

namespace gfx {

// Static factory
SurfaceBuilder SurfaceBuilder::create(int w, int h, PaletteHandle pal,
                                      uint8_t fill_idx) {
  Surface s(w, h, pal);
  s.fill_rect({0, 0, w, h}, fill_idx);
  return SurfaceBuilder(std::move(s));
}

SurfaceBuilder& SurfaceBuilder::blit(const Surface& src, ViewPoint dst,
                                     bool masked, uint8_t transparent) {
  if (masked) {
    surface_.blit_masked(src, dst, transparent);
  }
  else {
    surface_.blit_raw(src, dst);
  }
  return *this;
}

SurfaceBuilder& SurfaceBuilder::box(int x1, int y1, int x2, int y2,
                                    uint8_t color, int thickness) {
  draw::box(surface_, {x1, y1}, {x2, y2}, color, thickness);
  return *this;
}

SurfaceBuilder& SurfaceBuilder::line(ViewPoint p1, ViewPoint p2, uint8_t color,
                                     int thickness) {
  draw::line(surface_, p1, p2, color, thickness);
  return *this;
}

SurfaceBuilder& SurfaceBuilder::line(int x1, int y1, int x2, int y2,
                                     uint8_t color, int thickness) {
  Logger::warn("Deprecated: SurfaceBuilder::line without ViewPoint");
  draw::line(surface_, {x1, y1}, {x2, y2}, color, thickness);
  return *this;
}

SurfaceBuilder& SurfaceBuilder::fill(uint8_t color) {
  surface_.fill(color);
  return *this;
}

SurfaceBuilder& SurfaceBuilder::fill_rect(ViewRect r, uint8_t color) {
  surface_.fill_rect(r, color);
  return *this;
}

SurfaceBuilder& SurfaceBuilder::put(ViewPoint p, uint8_t idx) {
  surface_.put(p, idx);
  return *this;
}

} // namespace gfx
