/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include "engine/ui/TooltipRequest.h"

// Game code calls show()/clear(). TooltipLayer reads visible()/current() each
// frame.
class TooltipManager {
public:
  void delay(int ms) noexcept { delay_ms_ = ms; }

  void show(TooltipRequest req) {
    if (delay_ms_ <= 0 || req.anchor != TooltipAnchor::Mouse) {
      current_ = std::move(req);
      pending_.reset();
      touch();
      return;
    }
    pending_ = std::move(req);
    waited_ms_ = 0;
    if (current_) {
      current_.reset();
      touch();
    }
  }

  void clear() {
    pending_.reset();
    if (current_) {
      current_.reset();
      touch();
    }
  }

  void advance(uint32_t dt_ms) {
    if (!pending_) {
      return;
    }
    waited_ms_ += dt_ms;
    if (waited_ms_ < static_cast<uint32_t>(delay_ms_)) {
      return;
    }
    current_ = std::move(pending_);
    pending_.reset();
    touch();
  }

  // A pending tooltip waits for the cursor to stop; an active one stays where
  // it appeared.
  void update_mouse_pos(ViewPoint p) {
    if (pending_) {
      if (pending_->pos != p) {
        waited_ms_ = 0;
      }
      pending_->pos = p;
    }
  }

  bool visible() const { return current_.has_value(); }
  const TooltipRequest& current() const { return *current_; }
  uint64_t revision() const noexcept { return revision_; }

private:
  void touch() { ++revision_; }

  std::optional<TooltipRequest> current_;
  std::optional<TooltipRequest> pending_;
  uint32_t waited_ms_{0};
  int delay_ms_{0};
  uint64_t revision_{0};
};
