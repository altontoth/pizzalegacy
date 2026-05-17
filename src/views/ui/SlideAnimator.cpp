/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/SlideAnimator.h"
#include <algorithm>

// Constructor: Creates a slide animator that animates a card between two
// positions:
//   - screen_height: The Y position when fully hidden (off-screen at bottom)
//   - target_offset: The Y position when fully visible (on-screen at top)
SlideAnimator::SlideAnimator(int screen_height, int target_offset)
    : screen_height_(screen_height), target_offset_(target_offset),
      offset_(screen_height), state_(SlideState::HIDDEN) {}

// Initiates a slide-in animation from bottom (off-screen) to visible position.
// Sets state to APPEARING and resets offset to screen_height (starting
// position).
void SlideAnimator::start_show() {
  state_ = SlideState::APPEARING;
  offset_ = screen_height_;
}

// Initiates a slide-out animation from visible position to bottom (off-screen).
// Sets state to DISAPPEARING and ensures we start from the target_offset.
void SlideAnimator::start_hide() {
  state_ = SlideState::DISAPPEARING;
  // make sure we start at visible_y_
  offset_ = target_offset_;
}

// Updates the animation state. Called each frame.
// Returns true if the animator did something (state changed), false if idle.
//
// Appearing: Slides card up from bottom (decreasing Y from screen_height toward
// target_offset).
//   Uses easing: step size is proportional to distance, minimum 4 pixels.
//   When target is reached, snaps to visible.
//
// Disappearing: Slides card down to bottom (increasing Y from target_offset
// toward screen_height).
//   Uses easing: step size is proportional to distance, minimum 4 pixels.
//   When screen_height is reached, snaps to hidden.
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

// Returns the current Y offset for rendering the foreground card.
int SlideAnimator::offset() const { return offset_; }

SlideAnimator::SlideState SlideAnimator::state() const { return state_; }

bool SlideAnimator::is_visible() const { return state_ != SlideState::HIDDEN; }

bool SlideAnimator::is_animating() const {
  return state_ == SlideState::APPEARING || state_ == SlideState::DISAPPEARING;
}

// Immediately resets to HIDDEN state with no animation.
// Card is placed off-screen at screen_height.
void SlideAnimator::reset() {
  state_ = SlideState::HIDDEN;
  offset_ = screen_height_;
  bg_visible_ = false;
}

// Initiates a "push" animation: a new foreground card slides up from the bottom
// over an existing background card.
//
// Usage: Move current fg_card to bg_card, create new fg_card, then call push().
// The new fg_card will animate up from bottom to target_offset,
// with bg_card visible underneath.
void SlideAnimator::push() {
  bg_visible_ = true;
  start_show();
}

// Initiates a "pop" animation: the foreground card slides down off-screen,
// revealing the background card underneath. Used for navigating backward
//
// Usage: Place previous card in bg_card, then call pop().
// The fg_card will slide down off-screen (back to bottom), and when animation
// completes, bg_card becomes the new fg_card (handled in tick()).
void SlideAnimator::pop() {
  bg_visible_ = true; // Ensure bg is visible for the pop animation
  start_hide();
}

// Returns true if a background card should be rendered (during push/pop
// animations).
bool SlideAnimator::bg_visible() const { return bg_visible_; }

// "Snap" means instantly jump to a position without animating.
// This snaps the card to the fully visible position (target_offset) and
// sets state to VISIBLE. Called when APPEARING animation completes.
void SlideAnimator::snap_visible() {
  offset_ = target_offset_;
  state_ = SlideState::VISIBLE;
  bg_visible_ = false; // push complete, bg no longer needed for rendering
}

// Snaps to the hidden state when DISAPPEARING animation completes.
// Has special behavior depending on whether this is a pop() or normal hide:
//
// Case 1: pop() completion (bg_visible_ is true)
//   The foreground card has slid off-screen downward (back to bottom).
//   The background card was already at target_offset and should now become the
//   visible card.
//   - Clear bg_visible_ (no background anymore)
//   - Set offset to target_offset (the bg card position)
//   - Set state to VISIBLE (bg card is now showing as fg)
//   The caller's tick() will then move bg_card to fg_card.
//
// Case 2: Normal hide completion (bg_visible_ is false)
//   Simple slide-out animation finished. Card slid down to bottom and is now
//   off-screen.
//   - Set offset to screen_height (off-screen at bottom)
//   - Set state to HIDDEN
void SlideAnimator::snap_hidden() {
  if (bg_visible_) {
    // pop() just finished - foreground card left screen,
    // background card is already parked at target_offset_.
    bg_visible_ = false;      // nothing underneath anymore
    offset_ = target_offset_; // ensure on-screen
    state_ = SlideState::VISIBLE;
    return;
  }

  // normal slide-down / cancel completed
  offset_ = screen_height_;
  state_ = SlideState::HIDDEN;
}
