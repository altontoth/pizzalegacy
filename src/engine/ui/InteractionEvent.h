/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/geom/Spaces.h"

namespace views::ui {

enum class MouseButton { Left, Right };

struct InteractionEvent {
  enum class Type {
    Enter, // cursor entered rect
    Leave, // cursor left rect
    Hover, // cursor moved inside rect

    ButtonDown,  // button pressed inside rect
    ButtonUp,    // matching release (may be outside)
    Click,       // down + up on same rect
    Cancelled,   // up outside rect after down inside
    DoubleClick, // up twice on the same point(-ish)
    Scroll,      // mouse wheel scrolled inside rect

    DragStart, // movement crossed the drag threshold after a press
    DragMove,  // pointer moved during the drag
    DragEnd,   // drag finished
  };

  Type type;
  MouseButton button;
  ViewPoint pos;
  ViewPoint local;
  // For Scroll events: positive = up/away, negative = down/toward:
  int scroll_delta = 0;
  // For Scroll events on a sideways wheel: positive = right, negative = left.
  // macOS reports shift + vertical wheel here too.
  int scroll_delta_x = 0;
  // For DragMove: pointer movement since the previous frame:
  ViewPoint delta;
};

} // namespace views::ui
