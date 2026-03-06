/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

/* For the DropdownList we need a way to differentiate between a click
 * that should result in a screen being redrawn, or if it should result
 * in stopping propagation of the click (if they click another dropdown
 * list while one is open, we need the first one to realize there was
 * a click outside of its area (redraw) and let the other dropdown box
 * handle the click to draw itself (propagate). But if a user clicks
 * inside a dropdown entry at a spot that happens to be above the catchment
 * area of second dropdown list, it should redraw the screen but not propoagate.
 */
struct [[nodiscard]] ClickResult {
  // Stop further propagation of the clicks
  bool consumed = false;
  // Something on screen changed:
  bool mark_dirty = false;

  static constexpr ClickResult none() { return {}; }
  static constexpr ClickResult only_mark_dirty() {
    return {.consumed = false, .mark_dirty = true};
  }
  static constexpr ClickResult only_consumed() {
    return {.consumed = true, .mark_dirty = false};
  }
  static constexpr ClickResult both() {
    return {.consumed = true, .mark_dirty = true};
  }
};

constexpr ClickResult operator|(ClickResult lhs, ClickResult rhs) {
  return {.consumed = lhs.consumed || rhs.consumed,
          .mark_dirty = lhs.mark_dirty || rhs.mark_dirty};
}
