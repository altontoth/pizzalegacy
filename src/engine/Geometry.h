
/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/Size.h"
#include "engine/geom/Spaces.h"

// Placement of a box inside a container (for text use TextAnchor).
enum class HAlign { Left, Center, Right };
enum class VAlign { Top, Center, Bottom };

// If the box is larger than its container it'll extend beyond the right or
// bottom edge.
constexpr ViewPoint align_in_rect(const ViewRect& container, const Size& sz,
                                  HAlign h_align, VAlign v_align) {
  ViewPoint p{};
  switch (h_align) {
  case HAlign::Left:
    p.x = container.x;
    break;
  case HAlign::Center:
    p.x = container.x + (container.w - sz.w) / 2;
    break;
  case HAlign::Right:
    p.x = container.x + (container.w - sz.w);
    break;
  }

  switch (v_align) {
  case VAlign::Top:
    p.y = container.y;
    break;
  case VAlign::Center:
    p.y = container.y + (container.h - sz.h) / 2;
    break;
  case VAlign::Bottom:
    p.y = container.y + (container.h - sz.h);
    break;
  }

  return p;
}
