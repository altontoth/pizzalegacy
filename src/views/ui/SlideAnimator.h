/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

class SlideAnimator {
public:
  enum class SlideState {
    HIDDEN,
    APPEARING,
    VISIBLE,
    DISAPPEARING,
  };

  // step = remaining_distance / DISTANCE_DIVISOR each frame.
  // Smaller = faster start and steeper deceleration.
  static constexpr int DISTANCE_DIVISOR = 6;

  // Minimum pixels moved per frame, used near the end of the slide.
  // Larger = faster final approach.
  static constexpr int MIN_STEP_PX = 8;

  SlideAnimator(int screen_height, int target_offset);
  void start_show();
  void start_hide();
  bool update();
  int offset() const;
  SlideState state() const;
  bool on_screen() const;
  bool is_animating() const;
  void reset();
  void push();
  void pop();
  bool show_background() const;

private:
  void snap_visible();
  void snap_hidden();
  int screen_height_;
  int target_offset_;
  int offset_;
  SlideState state_;
  // A background card sits under the foreground one during a push or pop.
  bool show_background_{false};
  // True only during a pop: when the foreground finishes sliding off, the
  // background is promoted to foreground instead of the animator going hidden.
  bool revealing_background_{false};
};
