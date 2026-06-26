/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/SlideAnimator.h"
#include <algorithm>

// screen_height is the off-screen (hidden) Y; target_offset is the on-screen Y.
SlideAnimator::SlideAnimator(int screen_height, int target_offset)
    : screen_height_(screen_height), target_offset_(target_offset),
      offset_(screen_height), state_(SlideState::HIDDEN) {}

void SlideAnimator::start_show() {
  state_ = SlideState::APPEARING;
  offset_ = screen_height_;
  show_background_ = false;
  revealing_background_ = false;
}

void SlideAnimator::start_hide() {
  state_ = SlideState::DISAPPEARING;
  offset_ = target_offset_;
  show_background_ = false;
  revealing_background_ = false;
}

bool SlideAnimator::update() {
  switch (state_) {
  case SlideState::APPEARING: {
    int step =
        std::max(MIN_STEP_PX, (offset_ - target_offset_) / DISTANCE_DIVISOR);
    offset_ -= step;
    if (offset_ <= target_offset_) {
      snap_visible();
    }
    return true;
  }
  case SlideState::DISAPPEARING: {
    int step =
        std::max(MIN_STEP_PX, (screen_height_ - offset_) / DISTANCE_DIVISOR);
    offset_ += step;
    if (offset_ >= screen_height_) {
      snap_hidden();
    }
    return true;
  }
  default:
    return false;
  }
}

int SlideAnimator::offset() const { return offset_; }

SlideAnimator::SlideState SlideAnimator::state() const { return state_; }

bool SlideAnimator::on_screen() const { return state_ != SlideState::HIDDEN; }

bool SlideAnimator::is_animating() const {
  return state_ == SlideState::APPEARING || state_ == SlideState::DISAPPEARING;
}

void SlideAnimator::reset() {
  state_ = SlideState::HIDDEN;
  offset_ = screen_height_;
  show_background_ = false;
  revealing_background_ = false;
}

// Move the current foreground card to the background, build a new foreground
// card, then push(): the new card slides up over the old one, which is
// discarded once the slide finishes.
void SlideAnimator::push() {
  start_show();
  show_background_ = true;
}

// Put the card to return to in the background, then pop(): the foreground
// card slides off and the background is promoted to foreground.
void SlideAnimator::pop() {
  start_hide();
  show_background_ = true;
  revealing_background_ = true;
}

bool SlideAnimator::show_background() const { return show_background_; }

void SlideAnimator::snap_visible() {
  offset_ = target_offset_;
  state_ = SlideState::VISIBLE;
  show_background_ = false;
}

void SlideAnimator::snap_hidden() {
  if (revealing_background_) {
    // The background card is already parked at target_offset_ and becomes the
    // visible card now that the foreground has left the screen.
    offset_ = target_offset_;
    state_ = SlideState::VISIBLE;
  }
  else {
    offset_ = screen_height_;
    state_ = SlideState::HIDDEN;
  }
  show_background_ = false;
  revealing_background_ = false;
}
