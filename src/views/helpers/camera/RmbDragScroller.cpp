/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/helpers/camera/RmbDragScroller.h"

RmbDragScroller::~RmbDragScroller() {
  // Release the cursor lock if the view is destroyed mid-drag, although I can't
  // imagine how that would happen exactly.
  if (dragging_) {
    cursor_lock_fn_(false);
  }
}

void RmbDragScroller::init(CameraController& cc, Predicate can_drag,
                           Predicate is_inverted, OnClick on_click,
                           CursorLockFn cursor_lock_fn) {
  cc_ = &cc;
  can_drag_ = std::move(can_drag);
  is_inverted_ = std::move(is_inverted);
  on_click_ = std::move(on_click);
  cursor_lock_fn_ = std::move(cursor_lock_fn);
}

void RmbDragScroller::add_interaction(views::ui::InteractionLayers& layers,
                                      ViewRect viewport,
                                      std::function<void()> mark_dirty) {
  layers_ = &layers;
  mark_dirty_ = std::move(mark_dirty);

  using namespace views::ui;
  // Register over the viewport, with right-button drag enabled.
  layers.create_persistent_item(
      viewport, "rmb_drag_scroller",
      [this](const InteractionEvent& ev) { on_event(ev); }, {},
      InteractionLayer::Background, MouseButton::Right);
}

void RmbDragScroller::on_event(const views::ui::InteractionEvent& ev) {
  using views::ui::InteractionEvent;
  switch (ev.type) {
  case InteractionEvent::Type::DragStart:
    on_drag_start(ev);
    break;
  case InteractionEvent::Type::DragMove:
    on_drag_move(ev);
    break;
  case InteractionEvent::Type::DragEnd:
    on_drag_end(ev);
    break;
  case InteractionEvent::Type::Click:
    on_click(ev);
    break;
  default:
    break;
  }
}

void RmbDragScroller::on_drag_start(const views::ui::InteractionEvent& /*ev*/) {
  if (!can_drag_()) {
    layers_->cancel_drag();
    return;
  }
  dragging_ = true;
  cursor_lock_fn_(true);
}

void RmbDragScroller::on_drag_move(const views::ui::InteractionEvent& ev) {
  if (!dragging_) {
    return;
  }
  if (cc_->apply_drag_delta(ev.delta.x, ev.delta.y, is_inverted_())) {
    mark_dirty_();
  }
}

void RmbDragScroller::on_drag_end(const views::ui::InteractionEvent& /*ev*/) {
  if (!dragging_) {
    return;
  }
  cursor_lock_fn_(false);
  dragging_ = false;
}

void RmbDragScroller::on_click(const views::ui::InteractionEvent& ev) {
  if (ev.button != views::ui::MouseButton::Right) {
    return;
  }
  if (!can_drag_()) {
    return;
  }
  on_click_(ev.pos);
}
