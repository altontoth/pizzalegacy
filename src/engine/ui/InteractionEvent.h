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
  };

  Type type;
  MouseButton button;
  ViewPoint pos;
  ViewPoint local;
  int scroll_delta =
      0; // For Scroll events: positive = up/away, negative = down/toward
};

} // namespace views::ui
