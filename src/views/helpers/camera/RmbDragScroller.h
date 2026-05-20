/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>

#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionEvent.h"
#include "engine/ui/InteractionLayers.h"
#include "views/helpers/camera/CameraController.h"

/*
 * RmbDragScroller adds RMB drag-to-scroll to a viewport interaction
 *
 * View-specific behaviour is injected via callbacks:
 *
 *   can_drag:
 *   returns true when a drag should start (e.g. !on_cursor_)
 *
 *   is_inverted:
 *   returns true for drag-map style (content follows finger);
 *   false for swipe style (view follows drag direction)
 *
 *   on_click:
 *   called with the ButtonDown absolute position when the button
 *   is released without significant movement; we can use this to
 *   trigger the original RMB click action (zoom_out, etc.)
 *
 *   cursor_lock_fn:
 *   called with true at drag start to lock and hide the cursor, and with
 *   false at drag end to restore it
 */
class RmbDragScroller {
public:
  using Predicate = std::function<bool()>;
  using OnClick = std::function<void(ViewPoint abs_pos)>;
  using CursorLockFn = std::function<void(bool enable)>;

  // How far the oversized interaction rect reaches from centre (see
  // add_interaction). Game.cpp clamps the virtual cursor to stay inside it.
  static constexpr int DRAG_REACH = 30000;

  RmbDragScroller() = default;
  ~RmbDragScroller();

  // Call once before add_interaction.
  void init(CameraController& cc, Predicate can_drag, Predicate is_inverted,
            OnClick on_click, CursorLockFn cursor_lock_fn);

  void add_interaction(views::ui::InteractionLayers& layers, ViewRect viewport,
                       std::function<void()> mark_dirty);

private:
  static constexpr int CLICK_THRESHOLD = 4;

  void on_event(const views::ui::InteractionEvent& ev);
  void on_button_down(const views::ui::InteractionEvent& ev);
  void on_hover(const views::ui::InteractionEvent& ev);
  void on_button_up(const views::ui::InteractionEvent& ev);

  CameraController* cc_ = nullptr;
  Predicate can_drag_;
  Predicate is_inverted_;
  OnClick on_click_;
  CursorLockFn cursor_lock_fn_;

  ViewRect viewport_;
  std::function<void()> mark_dirty_;

  bool active_ = false;
  bool cursor_locked_ = false;
  bool any_scroll_ = false;
  ViewPoint start_abs_pos_;
  ViewPoint last_pos_;
  int total_moved_ = 0;
};
