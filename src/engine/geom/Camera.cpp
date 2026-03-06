/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/geom/Camera.h"
#include "engine/geom/GeomFormat.h"

#include <algorithm>

void Camera::view_size(Size s) noexcept {
  view_size_ = s;
  clamp_scroll();
}

void Camera::world_size(Size s) noexcept {
  world_size_ = s;
  clamp_scroll();
}

void Camera::align(Align ax, Align ay) noexcept {
  align_x_ = ax;
  align_y_ = ay;
}

void Camera::scroll(WorldPoint p) noexcept {
  scroll_ = p;
  clamp_scroll();
}

void Camera::zoom(int zoom_factor) noexcept {
  zoom_ = std::max(1, zoom_factor);
  clamp_scroll();
}

void Camera::center_on(WorldPoint wp) noexcept {
  const int vw_world = view_size_.w * zoom_;
  const int vh_world = view_size_.h * zoom_;
  scroll_.x = wp.x - (vw_world / 2);
  scroll_.y = wp.y - (vh_world / 2);
  clamp_scroll();
}

WorldRect Camera::visible_world_rect() const noexcept {
  const int vw_world = std::min(world_size_.w, view_size_.w * zoom_);
  const int vh_world = std::min(world_size_.h, view_size_.h * zoom_);
  return WorldRect{scroll_.x, scroll_.y, vw_world, vh_world};
}

ViewPoint Camera::origin_offset_in_view() const noexcept {
  return ViewPoint{axis_offset_in_view(view_size_.w, world_size_.w, align_x_),
                   axis_offset_in_view(view_size_.h, world_size_.h, align_y_)};
}

bool Camera::possibly_visible(WorldRect r) const noexcept {
  return visible_world_rect().intersects(r);
}

ViewPoint Camera::world_to_view(WorldPoint wp) const noexcept {
  const ViewPoint o = origin_offset_in_view();
  return ViewPoint{((wp.x - scroll_.x) / zoom_) + o.x,
                   ((wp.y - scroll_.y) / zoom_) + o.y};
}

WorldPoint Camera::view_to_world(ViewPoint vp) const noexcept {
  const ViewPoint o = origin_offset_in_view();
  return WorldPoint{((vp.x - o.x) * zoom_) + scroll_.x,
                    ((vp.y - o.y) * zoom_) + scroll_.y};
}

ViewRect Camera::world_to_view(WorldRect wr) const noexcept {
  const ViewPoint tl = world_to_view(WorldPoint{wr.x, wr.y});
  return ViewRect{tl.x, tl.y, wr.w / zoom_, wr.h / zoom_};
}

WorldRect Camera::view_to_world(ViewRect vr) const noexcept {
  const WorldPoint tl = view_to_world(ViewPoint{vr.x, vr.y});
  return WorldRect{tl.x, tl.y, vr.w * zoom_, vr.h * zoom_};
}

void Camera::clamp_scroll() noexcept {
  const int vw_world = view_size_.w * zoom_;
  const int vh_world = view_size_.h * zoom_;
  const int max_x = std::max(0, world_size_.w - vw_world);
  const int max_y = std::max(0, world_size_.h - vh_world);
  scroll_.x = std::clamp(scroll_.x, 0, max_x);
  scroll_.y = std::clamp(scroll_.y, 0, max_y);
}

int Camera::axis_offset_in_view(int view_len, int world_len,
                                Align a) const noexcept {
  const int world_len_in_view = world_len / std::max(1, zoom_);
  if (world_len_in_view >= view_len) {
    return 0;
  }
  const int pad = view_len - world_len_in_view;
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
