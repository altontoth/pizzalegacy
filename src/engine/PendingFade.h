/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <utility>
#include "core/state/FadePresenter.h"
#include "engine/DisplayConstants.h"

class PendingFade : public FadePresenter {
public:
  bool fade_through_black(int duration_frames,
                          std::function<void()> on_midpoint) override {
    duration_frames_ = duration_frames;
    on_midpoint_ = std::move(on_midpoint);
    requested_ = true;
    return true;
  }

  [[nodiscard]] bool requested() const { return requested_; }
  void clear_request() { requested_ = false; }
  [[nodiscard]] int duration_frames() const { return duration_frames_; }

  [[nodiscard]] bool has_midpoint() const {
    return static_cast<bool>(on_midpoint_);
  }
  void run_midpoint() {
    if (auto callback = std::exchange(on_midpoint_, nullptr)) {
      callback();
    }
  }

private:
  bool requested_{false};
  int duration_frames_{default_fade_length};
  std::function<void()> on_midpoint_;
};
