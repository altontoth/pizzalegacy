/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/ScrollBar.h"

#include <algorithm>
#include <cmath>

ViewRect ScrollBar::thumb_rect(ViewRect track, const Extent& extent) const {
  if (extent.total <= 0 || extent.visible >= extent.total) {
    return {};
  }

  const int max_scroll = extent.total - extent.visible;
  const int thumb_h =
      std::max(min_thumb_height, extent.visible * track.h / extent.total);
  int scroll = std::clamp(extent.offset, 0, max_scroll);
  if (style_.bottom_anchored) {
    scroll = max_scroll - scroll;
  }
  const int thumb_y = track.y + (scroll * (track.h - thumb_h) / max_scroll);
  return ViewRect{track.x + track.w - thumb_width - style_.margin, thumb_y,
                  thumb_width, thumb_h};
}

void ScrollBar::draw(gfx::Surface& surface, ViewRect track,
                     const Extent& extent) const {
  const ViewRect thumb = thumb_rect(track, extent);
  if (thumb.h > 0) {
    const bool lit = hovered_ || track_drag_.dragging();
    surface.fill_rect(thumb, lit ? style_.hover_color : style_.color);
  }
}

bool ScrollBar::in_column(ViewPoint pointer, ViewRect track, ViewRect thumb) {
  return pointer.x >= thumb.x - grab_padding && pointer.x < track.x + track.w &&
         pointer.y >= track.y && pointer.y < track.y + track.h;
}

bool ScrollBar::on_interaction(const views::ui::InteractionEvent& ie,
                               ViewPoint pointer, ViewRect track,
                               const Extent& extent, int& offset) {
  const ViewRect thumb = thumb_rect(track, extent);
  using Type = views::ui::InteractionEvent::Type;
  if (thumb.h == 0) {
    hovered_ = false;
    return false;
  }

  if (ie.type == Type::Enter || ie.type == Type::Hover ||
      ie.type == Type::Leave) {
    const bool hovered =
        ie.type != Type::Leave && in_column(pointer, track, thumb);
    const bool changed = hovered != hovered_;
    hovered_ = hovered;
    return changed;
  }

  if (ie.type == Type::ButtonDown && pointer.x < thumb.x - grab_padding) {
    track_drag_.reset();
    return false;
  }

  const auto result =
      track_drag_.on_interaction(ie, pointer.y,
                                 TrackDrag::Track{.start = track.y,
                                                  .length = track.h,
                                                  .handle_pos = thumb.y,
                                                  .handle_length = thumb.h});
  if (result.fraction) {
    const int max_scroll = extent.total - extent.visible;
    int scroll = static_cast<int>(
        std::lround(*result.fraction * static_cast<float>(max_scroll)));
    if (style_.bottom_anchored) {
      scroll = max_scroll - scroll;
    }
    offset = scroll;
  }
  return result.handled;
}
