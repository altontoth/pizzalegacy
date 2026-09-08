/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/SDL_include.h"
#include "engine/geom/Spaces.h"

namespace views::ui {

class InteractionManager;

// The mouse position in screen coordinates.
class PointerTracker {
public:
  PointerTracker(InteractionManager& interaction, int display_height)
      : interaction_(interaction), display_height_(display_height) {}

  // Keep the pointer at the same relative position when the screen height
  // changes.
  void display_height(int height);

  // An absolute move, clamped to the screen.
  void on_motion(const SDL_MouseMotionEvent& motion);

  // A locked pointer has no meaningful absolute position, so its relative
  // motion is applied to the virtual pointer instead.
  void on_relative_motion(const SDL_MouseMotionEvent& motion);

  void position(ScreenPoint p) { position_ = p; }
  [[nodiscard]] ScreenPoint position() const { return position_; }

  // Once a frame, after the event queue has been drained.
  void update_interaction();

private:
  InteractionManager& interaction_;
  ScreenPoint position_;
  int display_height_;
};

} // namespace views::ui
