/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/ui/InteractionManager.h"

#include <algorithm>
#include <cstdlib>

#include "engine/DisplayConstants.h"
#include "engine/SDL_include.h"

namespace views::ui {

// Handles items_ being modified during iteration (callbacks can destroy items).
void InteractionManager::process(const SDL_Event& e) {
  switch (e.type) {
  case SDL_MOUSEBUTTONDOWN: {
    ScreenPoint sp{e.button.x, e.button.y};
    ViewPoint p = screen_to_view_point(sp);
    MouseButton btn = to_button(e);

    ItemID normal_id = 0;
    std::vector<ItemID> background_ids;
    bool is_exclusive = false;

    for (const auto& item : items_) {
      if (!item.rect.contains(p)) {
        continue;
      }

      if (item.layer_type == InteractionLayer::Normal && normal_id == 0) {
        normal_id = item.id;
      }
      else if (item.layer_type == InteractionLayer::Exclusive) {
        if (normal_id == 0) {
          normal_id = item.id;
        }
        is_exclusive = true;
      }
      else if (item.layer_type == InteractionLayer::Background) {
        background_ids.push_back(item.id);
      }
    }

    if (normal_id != 0) {
      auto* item = lookup_item(normal_id);
      if (item) {
        if (btn == MouseButton::Left) {
          pressed_left_id_ = item->id;
          item->active_left = true;
        }
        else {
          pressed_right_id_ = item->id;
          item->active_right = true;
        }
        dispatch(*item, InteractionEvent::Type::ButtonDown, btn, p);
      }
    }

    if (!is_exclusive) {
      for (ItemID id : background_ids) {
        auto* item = lookup_item(id);
        if (item) {
          if (btn == MouseButton::Left) {
            item->active_left = true;
          }
          else {
            item->active_right = true;
          }
          dispatch(*item, InteractionEvent::Type::ButtonDown, btn, p);
        }
      }
    }

    // Arm a drag if the item registered itself as draggable with this mouse
    // button. The drag starts once the pointer moves past DRAG_THRESHOLD, until
    // then a release is a normal click.
    if (drag_source_ == 0) {
      ItemID drag_id = 0;
      if (normal_id != 0) {
        if (auto* it = lookup_item(normal_id); it && it->drag_button == btn) {
          drag_id = normal_id;
        }
      }
      if (drag_id == 0 && !is_exclusive) {
        for (ItemID id : background_ids) {
          if (auto* it = lookup_item(id); it && it->drag_button == btn) {
            drag_id = id;
            break;
          }
        }
      }
      if (drag_id != 0) {
        drag_source_ = drag_id;
        drag_button_ = btn;
        drag_start_pos_ = p;
        drag_active_ = false;
      }
    }
    break;
  }

  case SDL_MOUSEBUTTONUP: {
    ScreenPoint sp{e.button.x, e.button.y};
    ViewPoint p = screen_to_view_point(sp);
    MouseButton btn = to_button(e);
    const uint8_t clicks = e.button.clicks;

    // If a drag was in progress on this button, finish it and suppress the
    // click flow (a completed drag is not a click).
    if (drag_active_ && btn == drag_button_) {
      finish_drag(p);
      break;
    }
    // A press that never crossed the threshold: disarm it; the click flow below
    // delivers the normal Click.
    if (drag_source_ != 0 && btn == drag_button_) {
      drag_source_ = 0;
    }

    const ItemID normal_pressed_id =
        (btn == MouseButton::Left) ? pressed_left_id_ : pressed_right_id_;

    std::vector<ItemID> background_active_ids;
    for (const auto& item : items_) {
      if (item.layer_type != InteractionLayer::Background) {
        continue;
      }
      const bool is_active =
          (btn == MouseButton::Left) ? item.active_left : item.active_right;
      if (is_active) {
        background_active_ids.push_back(item.id);
      }
    }

    if (normal_pressed_id != 0) {
      auto* item = lookup_item(normal_pressed_id);
      if (item) {
        const bool inside = item->rect.contains(p);

        if (btn == MouseButton::Left) {
          item->active_left = false;
          pressed_left_id_ = 0;
        }
        else {
          item->active_right = false;
          pressed_right_id_ = 0;
        }

        dispatch(*item, InteractionEvent::Type::ButtonUp, btn, p);
        if (inside) {
          if (clicks >= 2) {
            dispatch(*item, InteractionEvent::Type::DoubleClick, btn, p);
          }
          dispatch(*item, InteractionEvent::Type::Click, btn, p);
        }
        else {
          dispatch(*item, InteractionEvent::Type::Cancelled, btn, p);
        }
      }
      else {
        if (btn == MouseButton::Left) {
          pressed_left_id_ = 0;
        }
        else {
          pressed_right_id_ = 0;
        }
      }
    }

    for (ItemID id : background_active_ids) {
      auto* item = lookup_item(id);
      if (item) {
        const bool inside = item->rect.contains(p);

        if (btn == MouseButton::Left) {
          item->active_left = false;
        }
        else {
          item->active_right = false;
        }

        dispatch(*item, InteractionEvent::Type::ButtonUp, btn, p);
        if (inside) {
          if (clicks >= 2) {
            dispatch(*item, InteractionEvent::Type::DoubleClick, btn, p);
          }
          dispatch(*item, InteractionEvent::Type::Click, btn, p);
        }
        else {
          dispatch(*item, InteractionEvent::Type::Cancelled, btn, p);
        }
      }
    }
    break;
  }

  case SDL_MOUSEWHEEL: {
    ViewPoint p = last_logical_mouse_pos_;

    ItemID normal_id = 0;
    std::vector<ItemID> background_ids;
    bool is_exclusive = false;

    for (const auto& item : items_) {
      if (!item.rect.contains(p)) {
        continue;
      }

      if (item.layer_type == InteractionLayer::Normal && normal_id == 0) {
        normal_id = item.id;
      }
      else if (item.layer_type == InteractionLayer::Exclusive) {
        if (normal_id == 0) {
          normal_id = item.id;
        }
        is_exclusive = true;
      }
      else if (item.layer_type == InteractionLayer::Background) {
        background_ids.push_back(item.id);
      }
    }

    int scroll_delta = e.wheel.y;

    if (normal_id != 0) {
      auto* item = lookup_item(normal_id);
      if (item) {
        InteractionEvent ie;
        ie.type = InteractionEvent::Type::Scroll;
        ie.button = MouseButton::Left;
        ie.pos = p;
        ie.local = ViewPoint{p.x - item->rect.x, p.y - item->rect.y};
        ie.scroll_delta = scroll_delta;
        item->callback(ie);
      }
    }

    if (!is_exclusive) {
      for (ItemID id : background_ids) {
        auto* item = lookup_item(id);
        if (item) {
          InteractionEvent ie;
          ie.type = InteractionEvent::Type::Scroll;
          ie.button = MouseButton::Left;
          ie.pos = p;
          ie.local = ViewPoint{p.x - item->rect.x, p.y - item->rect.y};
          ie.scroll_delta = scroll_delta;
          item->callback(ie);
        }
      }
    }
    break;
  }

  default:
    break;
  }
}

void InteractionManager::handle_tooltip(const InteractionItem& it,
                                        const InteractionEvent& ev) {
  if (!it.tooltip_cb) {
    return;
  }

  switch (ev.type) {
  case InteractionEvent::Type::Enter:
    tt_mgr_.show(it.tooltip_cb(ev));
    break;

  case InteractionEvent::Type::Hover:
    if (tt_mgr_.visible() && tt_mgr_.current().anchor == TooltipAnchor::Mouse) {
      // just update the mouse-anchored position
      tt_mgr_.update_mouse_pos(ev.pos);
    }
    break;

  case InteractionEvent::Type::Leave:
    tt_mgr_.clear();
    break;

  default:
    break;
  }
}

void InteractionManager::update_mouse(ScreenPoint sp) {
  ViewPoint p = screen_to_view_point(sp);
  last_logical_mouse_pos_ = p;

  ItemID found_normal = 0;
  bool is_exclusive = false;
  for (const auto& it : items_) {
    if (it.layer_type == InteractionLayer::Background) {
      continue;
    }
    if (it.rect.contains(p)) {
      if (found_normal == 0) {
        found_normal = it.id;
      }
      if (it.layer_type == InteractionLayer::Exclusive) {
        is_exclusive = true;
      }
    }
  }

  // If the drag source vanished, abandon the drag
  if (drag_source_ != 0 && lookup_item(drag_source_) == nullptr) {
    drag_active_ = false;
    drag_source_ = 0;
  }

  // Drag session: a press on a draggable item armed drag_source_.
  // This becomes an active drag once the pointer moves past the threshold.
  if (drag_source_ != 0 && !drag_active_) {
    const int moved =
        std::abs(p.x - drag_start_pos_.x) + std::abs(p.y - drag_start_pos_.y);
    if (moved > DRAG_THRESHOLD) {
      drag_active_ = true;
      drag_last_pos_ = p;
      if (auto* src = lookup_item(drag_source_)) {
        dispatch(*src, InteractionEvent::Type::DragStart, drag_button_, p);
      }
    }
  }

  // While a drag is active, suppress hover/enter/leave on every other item.
  // Forcing found_normal to 0 also fires a Leave on whatever was last hovered.
  if (drag_active_) {
    if (p.x != drag_last_pos_.x || p.y != drag_last_pos_.y) {
      if (auto* src = lookup_item(drag_source_)) {
        const ViewPoint delta{p.x - drag_last_pos_.x, p.y - drag_last_pos_.y};
        dispatch(*src, InteractionEvent::Type::DragMove, drag_button_, p,
                 delta);
      }
      drag_last_pos_ = p;
    }
    found_normal = 0;
    is_exclusive = false;
  }

  if (found_normal != current_hover_) {
    if (current_hover_ != 0) {
      if (auto* old = lookup_item(current_hover_)) {
        dispatch(*old, InteractionEvent::Type::Leave, MouseButton::Left, p);
      }
    }

    if (found_normal != 0) {
      if (auto* new_hot = lookup_item(found_normal)) {
        dispatch(*new_hot, InteractionEvent::Type::Enter, MouseButton::Left, p);
        hot_item_name_ = new_hot->name;
      }
    }

    current_hover_ = found_normal;
    if (on_hot_changed_) {
      on_hot_changed_();
    }
  }

  if (found_normal != 0) {
    if (auto* hot = lookup_item(found_normal)) {
      dispatch(*hot, InteractionEvent::Type::Hover, MouseButton::Left, p);
    }
  }

  // Collect background states upfront to avoid iterator invalidation
  if (!is_exclusive) {
    struct BgItemState {
      ItemID id;
      bool was_hovered;
      bool now_inside;
    };
    std::vector<BgItemState> bg_states;

    for (const auto& item : items_) {
      if (item.layer_type != InteractionLayer::Background) {
        continue;
      }
      bg_states.push_back({.id = item.id,
                           .was_hovered = item.hovered,
                           .now_inside = item.rect.contains(p)});
    }

    for (const auto& state : bg_states) {
      auto* item = lookup_item(state.id);
      if (!item) {
        continue;
      }

      if (state.now_inside != state.was_hovered) {
        if (state.now_inside) {
          dispatch(*item, InteractionEvent::Type::Enter, MouseButton::Left, p);
        }
        else {
          dispatch(*item, InteractionEvent::Type::Leave, MouseButton::Left, p);
        }
        item->hovered = state.now_inside;
      }

      if (state.now_inside) {
        dispatch(*item, InteractionEvent::Type::Hover, MouseButton::Left, p);
      }
    }
  }
}

void InteractionManager::dump_layers(std::ostream& os) const {
  os << "Interaction Layers\n";
  for (auto const& it : items_) {
    os << "ID: " << it.id << "  Layer: " << it.layer << "  Name: \"" << it.name
       << "\""
       << "  ViewRect: {x=" << it.rect.x << ", y=" << it.rect.y
       << ", w=" << it.rect.w << ", h=" << it.rect.h << "}\n";
  }
}

MouseButton InteractionManager::to_button(const SDL_Event& e) noexcept {
  return (e.button.button == SDL_BUTTON_LEFT) ? MouseButton::Left
                                              : MouseButton::Right;
}

void InteractionManager::dispatch(const InteractionItem& item,
                                  InteractionEvent::Type type, MouseButton btn,
                                  ViewPoint p, ViewPoint delta) const {

  // Copy callback before dispatching since the callback may destroy the item.
  const auto cb = item.callback;
  const auto local_pos = p - item.rect.point();
  const InteractionEvent ev{.type = type,
                            .button = btn,
                            .pos = p,
                            .local = ViewPoint{local_pos.dx, local_pos.dy},
                            .delta = delta};

  const_cast<InteractionManager*>(this)->handle_tooltip(item, ev);

  cb(ev);
}

void InteractionManager::finish_drag(ViewPoint p) {
  if (auto* src = lookup_item(drag_source_)) {
    dispatch(*src, InteractionEvent::Type::DragEnd, drag_button_, p);
  }
  // Clear press/active state for the drag button so nothing stays stuck
  // highlighted or pressed. We have to loop through them all because a single
  // press can activate several items (background items as well).
  for (auto& item : items_) {
    (drag_button_ == MouseButton::Left ? item.active_left : item.active_right) =
        false;
  }
  (drag_button_ == MouseButton::Left ? pressed_left_id_ : pressed_right_id_) =
      0;
  drag_active_ = false;
  drag_source_ = 0;
}

ViewPoint InteractionManager::screen_to_view_point(ScreenPoint sp) const {
  // Scale Y from screen space (e.g. 640x480) to view space (640x400)
  if (display_height_ == DisplayConstants::LOGICAL_HEIGHT) {
    return ViewPoint{sp.x, sp.y};
  }
  return ViewPoint{sp.x,
                   (sp.y * DisplayConstants::LOGICAL_HEIGHT) / display_height_};
}

} // namespace views::ui
