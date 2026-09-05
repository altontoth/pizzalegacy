/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>

#include "resources/util/FileUtils.h"

#include "engine/gfx/Surface.h"
#include "resources/BlitmapIcons.h"

using gfx::Surface;

// Explicit template instantiations
template class Blitmap<Blitmap1Icon>;
template class Blitmap<Blitmap2Icon>;
template class Blitmap<Blitmap3Icon>;
template class Blitmap<Blitmap4Icon>;

template <typename IconEnum>
Blitmap<IconEnum>::Blitmap(VgaManager* vga_manager, const std::string& vga_path,
                           const std::string& offsets_path)
    : surface_(vga_manager->get(vga_path).clone()) {

  std::ifstream f = fileutils::open(offsets_path);

  // Each blitmap has 30 entries x 4 bytes each = 120 bytes
  static constexpr int NUM_ENTRIES = 30;
  rects_.resize(NUM_ENTRIES);

  for (int i = 0; i < NUM_ENTRIES; ++i) {
    uint32_t val = 0;
    f.read(reinterpret_cast<char*>(&val), sizeof(val));
    if (!f) {
      // Not enough data
      rects_.resize(i); // keep partial
      break;
    }
    // Decompose and multiply for our 2x scale
    int x1 = (val & 0xFF) * 2;
    int y1 = ((val >> 8) & 0xFF) * 2;
    // We need one extra pixel because of the doubling of the
    // graphics file upon load
    int x2 = (((val >> 16) & 0xFF) * 2) + 1;
    int y2 = (((val >> 24) & 0xFF) * 2) + 1;

    // Build rect
    rects_[i] =
        ViewRect{std::min<int>(x1, x2), std::min<int>(y1, y2),
                 // Both width and height get another +1 to account for doubling
                 // the size, since a pixel painted at x2 and y2 is actually
                 // going to be painted at x2,y2, x2,y2+2, x2+1,y2 and x2+1,y2+1
                 std::abs(x2 - x1) + 1, std::abs(y2 - y1) + 1};
  }
  return;
}

template <typename IconEnum>
ViewRect Blitmap<IconEnum>::rect(IconEnum icon) const {
  return rect_internal(static_cast<int>(icon));
}

template <typename IconEnum>
void Blitmap<IconEnum>::render_icon(IconEnum icon, Surface& dest,
                                    ViewPoint pos) const {
  render_icon_internal(static_cast<int>(icon), dest, pos);
}

template <typename IconEnum>
Surface Blitmap<IconEnum>::icon_surface(IconEnum icon) const {
  return icon_surface(static_cast<int>(icon));
}

// Internal helpers
template <typename IconEnum>
ViewRect Blitmap<IconEnum>::rect_internal(int index) const {
  if (index < 0 || index >= static_cast<int>(rects_.size())) {
    return ViewRect{0, 0, 0, 0};
  }
  return rects_[index];
}

template <typename IconEnum>
void Blitmap<IconEnum>::render_icon_internal(int index, Surface& dest,
                                             ViewPoint pos) const {
  if (index < 0 || index >= static_cast<int>(rects_.size())) {
    return;
  }
  const ViewRect& src = rects_[index];
  dest.blit(surface_, src, pos);
}

template <typename IconEnum>
Surface Blitmap<IconEnum>::icon_surface(int index) const {
  ViewRect rect = rect_internal(index);
  Surface surf(rect.w, rect.h);
  render_icon_internal(index, surf, ViewPoint{0, 0});
  return surf;
}

template <typename IconEnum>
void Blitmap<IconEnum>::render_icon_aligned(IconEnum icon, Surface& dest,
                                            HAlign h_align, VAlign v_align,
                                            std::optional<ViewRect> bounds,
                                            ViewPoint offset) const {
  int index = static_cast<int>(icon);
  if (index < 0 || index >= static_cast<int>(rects_.size())) {
    return;
  }

  const ViewRect& src = rects_[index];
  // full-surface as default container
  ViewRect container =
      bounds.value_or(ViewRect{0, 0, dest.width(), dest.height()});

  ViewPoint pos = align_in_rect(container, src.size(), h_align, v_align);

  // then apply any extra offset
  pos.x += offset.x;
  pos.y += offset.y;

  render_icon_internal(index, dest, pos);
}
