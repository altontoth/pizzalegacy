/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>

#include "engine/Size.h"
#include "engine/geom/Spaces.h"

struct ViewportTransform {
  Size view_size{0, 0};
  Size world_size{0, 0};
  // camera top-left in world space
  WorldPoint scroll{0, 0};
  int zoom{1};

  enum class Align { Start, Center, End };

  Align align_x{Align::Start};
  Align align_y{Align::Start};

  [[nodiscard]] ViewPoint world_to_view(WorldPoint wp) const noexcept {
    const ViewPoint o = origin_offset_in_view();
    return ViewPoint{((wp.x - scroll.x) / zoom) + o.x,
                     ((wp.y - scroll.y) / zoom) + o.y};
  }

  [[nodiscard]] WorldPoint view_to_world(ViewPoint vp) const noexcept {
    const ViewPoint o = origin_offset_in_view();
    return WorldPoint{((vp.x - o.x) * zoom) + scroll.x,
                      ((vp.y - o.y) * zoom) + scroll.y};
  }

  [[nodiscard]] ViewRect world_to_view(WorldRect wr) const noexcept {
    const ViewPoint tl = world_to_view(WorldPoint{wr.x, wr.y});
    return ViewRect{tl.x, tl.y, wr.w / zoom, wr.h / zoom};
  }

  [[nodiscard]] WorldRect view_to_world(ViewRect vr) const noexcept {
    const WorldPoint tl = view_to_world(ViewPoint{vr.x, vr.y});
    return WorldRect{tl.x, tl.y, vr.w * zoom, vr.h * zoom};
  }

  void set_view_size(Size s) noexcept {
    view_size = s;
    clamp_scroll();
  }

  void set_world_size(Size s) noexcept {
    world_size = s;
    clamp_scroll();
  }

  void set_zoom(int z) noexcept {
    z = std::max(z, 1);
    zoom = z;
    clamp_scroll();
  }

  void set_align(Align ax, Align ay) noexcept {
    align_x = ax;
    align_y = ay;
  }

  void clamp_scroll() noexcept {
    const int vw_world = view_size.w * zoom;
    const int vh_world = view_size.h * zoom;

    if (world_size.w <= vw_world) {
      scroll.x = 0;
    }
    else {
      scroll.x = std::max(scroll.x, 0);
      scroll.x = std::min(scroll.x, world_size.w - vw_world);
    }

    if (world_size.h <= vh_world) {
      scroll.y = 0;
    }
    else {
      scroll.y = std::max(scroll.y, 0);
      scroll.y = std::min(scroll.y, world_size.h - vh_world);
    }
  }

  void center_on(WorldPoint wp) noexcept {
    const int vw_world = view_size.w * zoom;
    const int vh_world = view_size.h * zoom;
    scroll.x = wp.x - vw_world / 2;
    scroll.y = wp.y - vh_world / 2;
    clamp_scroll();
  }

  [[nodiscard]] int snap_to_tile(int v, int tile) const noexcept {
    if (tile <= 0) {
      return v;
    }
    return (v / tile) * tile;
  }

  [[nodiscard]] WorldRect visible_world_rect() const noexcept {
    const int vw_world = std::min(world_size.w, view_size.w * zoom);
    const int vh_world = std::min(world_size.h, view_size.h * zoom);
    return WorldRect{scroll.x, scroll.y, vw_world, vh_world};
  }

  [[nodiscard]] bool possibly_visible(WorldRect r) const noexcept {
    return intersect_rect(r, visible_world_rect()).has_value();
  }

  [[nodiscard]] ViewPoint origin_offset_in_view() const noexcept {
    return ViewPoint{axis_offset(view_size.w, world_size.w, align_x),
                     axis_offset(view_size.h, world_size.h, align_y)};
  }

private:
  int axis_offset(int view_len, int world_len, Align a) const noexcept {
    const int world_in_view = world_len / std::max(1, zoom);
    if (world_in_view >= view_len) {
      return 0;
    }
    const int pad = view_len - world_in_view;
    switch (a) {
    case Align::Start:
      return 0;
    case Align::Center:
      return pad / 2;
    case Align::End:
      return pad;
    }
    return 0;
  }
};
