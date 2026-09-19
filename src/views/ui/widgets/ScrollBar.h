/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <optional>

#include "engine/geom/Spaces.h"
#include "engine/gfx/Surface.h"
#include "engine/ui/InteractionEvent.h"
#include "views/ui/widgets/TrackDrag.h"

// A scroll thumb along the right edge of a list, which can be dragged or
// moved by pressing the track. Offsets count whatever unit the list scrolls
// by; the thumb only shows when the content does not fit.
class ScrollBar {
public:
  struct Style {
    uint8_t color = 12;
    // While the pointer is over the thumb's column or dragging it.
    uint8_t hover_color = 10;
    // The thin line that the thumb runs along, to show how much there is to
    // scroll.
    std::optional<uint8_t> track_color = 14;
    // Gap between the thumb and the track's right edge.
    int margin = 1;
    // Offset 0 shows the end of the content rather than its start.
    bool bottom_anchored = false;
  };

  struct Extent {
    int offset = 0;
    int visible = 0;
    int total = 0;
  };

  ScrollBar() = default;
  explicit ScrollBar(Style style) : style_(style) {}

  // The track is in the same coordinates as the surface drawn on.
  void draw(gfx::Surface& surface, ViewRect track, const Extent& extent) const;

  // Pointer events in the track's coordinates. Returns whether the event was
  // for the scroll bar, and writes any new offset.
  bool on_interaction(const views::ui::InteractionEvent& ie, ViewPoint pointer,
                      ViewRect track, const Extent& extent, int& offset);

private:
  static constexpr int thumb_width = 4;
  static constexpr int min_thumb_height = 10;
  // How far left of the thumb a press still reaches the scroll bar.
  static constexpr int grab_padding = 6;

  // Thumb rect, or an empty rect when everything fits.
  ViewRect thumb_rect(ViewRect track, const Extent& extent) const;
  // Whether a pointer is over the column a press can reach the thumb in.
  static bool in_column(ViewPoint pointer, ViewRect track, ViewRect thumb);

  Style style_;
  TrackDrag track_drag_;
  bool hovered_ = false;
};
