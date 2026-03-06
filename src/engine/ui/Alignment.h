/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/Size.h"
#include "engine/geom/Spaces.h"

enum class AlignX {
  Left,       // x is left edge of text
  Center,     // x is left edge of container, text centered within it
  Right,      // x is left edge of container, text right-aligned
  RightEdge,  // x is right edge of text
  CenterPoint // x is center point of text
};
enum class AlignY { Top, Middle, Bottom };

namespace views::ui {

inline ViewPoint align_in_rect(const ViewRect& container, const Size& sz,
                               AlignX ax = AlignX::Center,
                               AlignY ay = AlignY::Top) {
  int x = container.x;
  switch (ax) {
  case AlignX::Left:
    break;
  case AlignX::Center:
    x += (container.w - sz.w) / 2;
    break;
  case AlignX::Right:
    x += (container.w - sz.w);
    break;
  case AlignX::RightEdge:
    x = container.x - sz.w;
    break;
  case AlignX::CenterPoint:
    x = container.x - (sz.w / 2);
    break;
  }

  int y = container.y;
  switch (ay) {
  case AlignY::Top:
    break;
  case AlignY::Middle:
    y += (container.h - sz.h) / 2;
    break;
  case AlignY::Bottom:
    y += (container.h - sz.h);
    break;
  }

  return ViewPoint{x, y};
};

} // namespace views::ui
