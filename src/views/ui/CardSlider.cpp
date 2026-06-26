/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/CardSlider.h"

#include <utility>

namespace views::ui {

CardSlider::CardSlider(int screen_height, int target_offset)
    : anim_(screen_height, target_offset), target_offset_(target_offset) {}

void CardSlider::present(gfx::Surface card) {
  foreground_ = std::move(card);
  background_.reset();
  pending_.reset();
  anim_.start_show();
}

void CardSlider::push(gfx::Surface card) {
  background_ = std::move(foreground_);
  foreground_ = std::move(card);
  anim_.push();
}

void CardSlider::pop(gfx::Surface previous) {
  background_ = std::move(previous);
  anim_.pop();
}

void CardSlider::replace(gfx::Surface card) {
  if (anim_.state() == SlideAnimator::SlideState::HIDDEN) {
    present(std::move(card));
    return;
  }
  pending_ = std::move(card);
  anim_.start_hide();
}

void CardSlider::dismiss() {
  pending_.reset();
  if (anim_.state() != SlideAnimator::SlideState::HIDDEN) {
    anim_.start_hide();
  }
}

void CardSlider::clear() {
  foreground_.reset();
  background_.reset();
  pending_.reset();
  anim_.reset();
}

void CardSlider::set_current(gfx::Surface card) {
  foreground_ = std::move(card);
}

bool CardSlider::update() {
  auto before = anim_.state();
  if (!anim_.update()) {
    return false;
  }

  switch (anim_.state()) {
  case SlideAnimator::SlideState::HIDDEN:
    foreground_.reset();
    background_.reset();
    if (pending_) {
      // replace(): the old card has left, now bring the queued one up.
      foreground_ = std::move(*pending_);
      pending_.reset();
      anim_.start_show();
    }
    else if (on_hidden_) {
      on_hidden_();
    }
    break;
  case SlideAnimator::SlideState::VISIBLE:
    if (background_) {
      // pop() promotes the revealed card; push() just drops the covered one.
      if (before == SlideAnimator::SlideState::DISAPPEARING) {
        foreground_ = std::move(*background_);
      }
      background_.reset();
    }
    break;
  default:
    break;
  }
  return true;
}

void CardSlider::render(gfx::Surface& dst, int x) const {
  if (anim_.show_background() && background_) {
    dst.blit_masked(*background_, {x, target_offset_});
  }
  if (anim_.state() != SlideAnimator::SlideState::HIDDEN && foreground_) {
    dst.blit_masked(*foreground_, {x, anim_.offset()});
  }
}

bool CardSlider::idle() const {
  return anim_.state() == SlideAnimator::SlideState::HIDDEN && !pending_;
}

bool CardSlider::on_screen() const { return anim_.on_screen(); }

bool CardSlider::settled() const {
  return anim_.state() == SlideAnimator::SlideState::VISIBLE;
}

bool CardSlider::animating() const { return anim_.is_animating(); }

void CardSlider::on_hidden(std::function<void()> cb) {
  on_hidden_ = std::move(cb);
}

} // namespace views::ui
