/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/TrackDrag.h"

#include <algorithm>

float TrackDrag::fraction_at(int pointer, const Track& track) const {
  const int travel = track.length - track.handle_length;
  if (travel <= 0) {
    return 0.0f;
  }
  const auto offset = static_cast<float>(pointer - grab_offset_ - track.start);
  return std::clamp(offset / static_cast<float>(travel), 0.0f, 1.0f);
}

TrackDrag::Result
TrackDrag::on_interaction(const views::ui::InteractionEvent& ie, int pointer,
                          const Track& track) {
  using Type = views::ui::InteractionEvent::Type;

  switch (ie.type) {
  case Type::ButtonDown: {
    if (ie.button != views::ui::MouseButton::Left || !track.contains(pointer)) {
      armed_ = false;
      return {};
    }
    armed_ = true;
    const bool on_handle = track.handle_length > 0 &&
                           pointer >= track.handle_pos &&
                           pointer < track.handle_pos + track.handle_length;
    if (on_handle) {
      grab_offset_ = pointer - track.handle_pos;
      return {.handled = true};
    }
    grab_offset_ = track.handle_length / 2;
    return {.handled = true, .fraction = fraction_at(pointer, track)};
  }
  case Type::DragStart:
    if (!armed_) {
      return {};
    }
    dragging_ = true;
    return {.handled = true, .fraction = fraction_at(pointer, track)};
  case Type::DragMove:
    if (!dragging_) {
      return {};
    }
    return {.handled = true, .fraction = fraction_at(pointer, track)};
  case Type::DragEnd:
    if (!dragging_ && !armed_) {
      return {};
    }
    dragging_ = false;
    armed_ = false;
    return {.handled = true};
  default:
    return {};
  }
}
