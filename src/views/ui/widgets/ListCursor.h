/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>

// Selection cursor over the rows of a list widget, shared by keyboard
// stepping and mouse hover. `active` is the row the cursor is on, or -1
// while the cursor has not been placed yet.
struct ListCursor {
  int active = -1;

  // For keyboard movement:
  // Move the cursor by delta (-1 or +1) within [0, count). When a scroll
  // offset is given, it is adjusted so the cursor stays within the window of
  // `visible` rows. Returns false when the cursor did not move (empty list,
  // or already at the end it was moving towards).
  bool step(int delta, int count, int* scroll = nullptr, int visible = 0) {
    if (count <= 0) {
      return false;
    }
    const int prev = active;
    if (active < 0) {
      active = delta > 0 ? 0 : count - 1;
    }
    else {
      active = std::clamp(active + delta, 0, count - 1);
    }
    if (scroll != nullptr && visible > 0) {
      if (active < *scroll) {
        *scroll = active;
      }
      else if (active >= *scroll + visible) {
        *scroll = active - visible + 1;
      }
    }
    return active != prev;
  }
};
