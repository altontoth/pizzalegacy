
/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cassert>
#include "engine/geom/Spaces.h"

enum class HAlign { Left, Center, Right };
enum class VAlign { Top, Center, Bottom };

constexpr ViewPoint align_in_rect(const ViewRect& container,
                                  const ViewRect& src, HAlign h_align,
                                  VAlign v_align) {
  // src.w, src.h are the width/height of the thing we're aligning
  // container.x, container.y, container.w, container.h define the box
  assert(src.w <= container.w);
  assert(src.h <= container.h);

  ViewPoint p{};
  switch (h_align) {
  case HAlign::Left:
    p.x = container.x;
    break;
  case HAlign::Center:
    p.x = container.x + (container.w - src.w) / 2;
    break;
  case HAlign::Right:
    p.x = container.x + (container.w - src.w);
    break;
  }

  switch (v_align) {
  case VAlign::Top:
    p.y = container.y;
    break;
  case VAlign::Center:
    p.y = container.y + (container.h - src.h) / 2;
    break;
  case VAlign::Bottom:
    p.y = container.y + (container.h - src.h);
    break;
  }

  return p;
}
