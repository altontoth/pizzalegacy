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
 * RmbDragScroller adds RMB drag-to-scroll to a viewport interaction.
 *
 * Registers a single Background interaction item over the viewport, made
 * draggable on the right mouse button.
 *
 * View-specific behaviour is injected via callbacks:
 *
 *   can_drag:
 *   returns true when an RMB interaction is allowed right now (e.g. QoL option
 *   on, no zoom selector open). When false, both drag and click do nothing.
 *
 *   is_inverted:
 *   returns true for drag-map style (content follows finger);
 *   false for swipe style (view follows drag direction)
 *
 *   on_click:
 *   called with the click position when the button is released without a drag;
 *   used to trigger the original RMB click action (zoom_out)
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

  RmbDragScroller() = default;
  ~RmbDragScroller();

  bool is_active() const { return dragging_; }

  // Call once before add_interaction.
  void init(CameraController& cc, Predicate can_drag, Predicate is_inverted,
            OnClick on_click, CursorLockFn cursor_lock_fn);

  void add_interaction(views::ui::InteractionLayers& layers, ViewRect viewport,
                       std::function<void()> mark_dirty);

private:
  void on_event(const views::ui::InteractionEvent& ev);
  void on_drag_start(const views::ui::InteractionEvent& ev);
  void on_drag_move(const views::ui::InteractionEvent& ev);
  void on_drag_end(const views::ui::InteractionEvent& ev);
  void on_click(const views::ui::InteractionEvent& ev);

  CameraController* cc_ = nullptr;
  Predicate can_drag_;
  Predicate is_inverted_;
  OnClick on_click_;
  CursorLockFn cursor_lock_fn_;

  views::ui::InteractionLayers* layers_ = nullptr;
  std::function<void()> mark_dirty_;

  bool dragging_ = false;
};
