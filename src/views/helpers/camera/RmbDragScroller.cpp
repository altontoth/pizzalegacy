/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/helpers/camera/RmbDragScroller.h"

#include <cstdlib>

RmbDragScroller::~RmbDragScroller() {
  // Release cursor lock if the view is destroyed while a drag is in progress,
  // although I can't imagine how that would happen exactly.
  if (cursor_locked_) {
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
  viewport_ = viewport;
  mark_dirty_ = std::move(mark_dirty);

  using namespace views::ui;
  // Use a much larger interaction rect so Hover events continue during drag
  // even when the cursor leaves the viewport. In relative mouse mode we
  // only get mouse deltas into a virtual position which can move outside
  // the viewport bounds.
  constexpr int K = DRAG_REACH;
  const ViewRect huge{-K, -K, (2 * K) + 1, (2 * K) + 1};
  layers.create_persistent_item(
      huge, "rmb_drag_scroller",
      [this](const InteractionEvent& ev) { on_event(ev); }, {},
      InteractionLayer::Background);
}

void RmbDragScroller::on_event(const views::ui::InteractionEvent& ev) {
  using views::ui::InteractionEvent;
  switch (ev.type) {
  case InteractionEvent::Type::ButtonDown:
    on_button_down(ev);
    break;
  case InteractionEvent::Type::Hover:
    on_hover(ev);
    break;
  case InteractionEvent::Type::ButtonUp:
  case InteractionEvent::Type::Cancelled:
    on_button_up(ev);
    break;
  default:
    break;
  }
}

void RmbDragScroller::on_button_down(const views::ui::InteractionEvent& ev) {
  if (ev.button != views::ui::MouseButton::Right ||
      !viewport_.contains(ev.pos) || !can_drag_()) {
    return;
  }
  active_ = true;
  start_abs_pos_ = ev.pos;
  last_pos_ = ev.local;
  total_moved_ = 0;
  any_scroll_ = false;
  // Lock and hide the cursor for the duration of the drag.
  cursor_lock_fn_(true);
  cursor_locked_ = true;
}

void RmbDragScroller::on_hover(const views::ui::InteractionEvent& ev) {
  if (!active_) {
    return;
  }
  const int dx = ev.local.x - last_pos_.x;
  const int dy = ev.local.y - last_pos_.y;
  if (dx == 0 && dy == 0) {
    return;
  }
  total_moved_ += std::abs(dx) + std::abs(dy);
  last_pos_ = ev.local;
  any_scroll_ |= cc_->apply_drag_delta(dx, dy, is_inverted_());
  mark_dirty_();
}

void RmbDragScroller::on_button_up(const views::ui::InteractionEvent& ev) {
  if (ev.button != views::ui::MouseButton::Right || !active_) {
    return;
  }
  if (cursor_locked_) {
    cursor_lock_fn_(false);
    cursor_locked_ = false;
  }
  if (!any_scroll_ && total_moved_ <= CLICK_THRESHOLD) {
    on_click_(start_abs_pos_);
  }
  active_ = false;
}
