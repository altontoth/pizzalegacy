/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/Size.h"
#include "engine/geom/Spaces.h"

// Camera = viewport geometry + mapping between world and view spaces.
class Camera {
public:
  enum class Align { Start, Center, End };

  Camera() = default;

  void view_size(Size s) noexcept;
  void world_size(Size s) noexcept;
  void align(Align ax, Align ay) noexcept;
  void scroll(WorldPoint p) noexcept;
  void zoom(int) noexcept;
  void center_on(WorldPoint wp) noexcept;

  [[nodiscard]] Size view_size() const noexcept { return view_size_; }
  [[nodiscard]] Size world_size() const noexcept { return world_size_; }
  [[nodiscard]] Align align_x() const noexcept { return align_x_; }
  [[nodiscard]] Align align_y() const noexcept { return align_y_; }
  [[nodiscard]] WorldPoint scroll() const noexcept { return scroll_; }
  [[nodiscard]] int zoom() const noexcept { return zoom_; }

  // If world < view on an axis, returns the entire world (scroll is 0).
  [[nodiscard]] WorldRect visible_world_rect() const noexcept;

  // View-space offset when world is smaller than view (Start/Center/End).
  [[nodiscard]] ViewPoint origin_offset_in_view() const noexcept;

  [[nodiscard]] bool possibly_visible(WorldRect r) const noexcept;

  [[nodiscard]] ViewPoint world_to_view(WorldPoint wp) const noexcept;
  [[nodiscard]] WorldPoint view_to_world(ViewPoint vp) const noexcept;

  [[nodiscard]] ViewRect world_to_view(WorldRect wr) const noexcept;
  [[nodiscard]] WorldRect view_to_world(ViewRect vr) const noexcept;

  // Snap a world coordinate down to the tile grid.
  [[nodiscard]] static int snap_to_tile(int v, int tile) noexcept {
    if (tile <= 0) {
      return v;
    }
    return (v / tile) * tile;
  }

private:
  void clamp_scroll() noexcept;
  [[nodiscard]] int axis_offset_in_view(int view_len, int world_len,
                                        Align a) const noexcept;

private:
  Size view_size_{0, 0};
  Size world_size_{0, 0};
  WorldPoint scroll_{0, 0};
  int zoom_{1};

  Align align_x_{Align::Start};
  Align align_y_{Align::Start};
};
