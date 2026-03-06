/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include "engine/ui/TooltipRequest.h"

// Game code calls show()/clear(). TooltipLayer reads visible()/current() each
// frame.
class TooltipManager {
public:
  void show(TooltipRequest req) {
    current_ = std::move(req);
    touch();
  }

  void clear() {
    current_.reset();
    touch();
  }

  // Update cursor position for mouse-anchored tooltips (e.g. on hover move)
  void update_mouse_pos(ViewPoint p) {
    if (current_ && current_->anchor == TooltipAnchor::Mouse) {
      current_->screenPos = p;
      touch();
    }
  }

  bool visible() const { return current_.has_value(); }
  const TooltipRequest& current() const { return *current_; }
  uint64_t revision() const noexcept { return revision_; }

private:
  void touch() { ++revision_; }

  std::optional<TooltipRequest> current_;
  uint64_t revision_{0};
};
