/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <optional>

#include "engine/gfx/Surface.h"
#include "views/ui/SlideAnimator.h"

namespace views::ui {

// Owns the foreground/background card surfaces and drives them with a
// SlideAnimator. Callers express intent (present/push/pop/replace/dismiss) and
// the surface bookkeeping happens in here.
//
// We can handle both styles:
//   - player profile: present / replace / dismiss
//   - staff view: push / pop (card slides over or reveals another)
class CardSlider {
public:
  // screen_height is the off-screen Y; target_offset is the resting on-screen
  // Y.
  CardSlider(int screen_height, int target_offset);

  // Slide a fresh card up from the bottom with nothing shown underneath.
  void present(gfx::Surface card);
  // Slide a new card up over the current one; the covered card is discarded
  // once the new one arrives.
  void push(gfx::Surface card);
  // Slide the current card away to reveal `previous`, which becomes the
  // foreground once the current card has left.
  void pop(gfx::Surface previous);
  // Slide the current card away, then slide `card` up in its place. Behaves
  // like present() when nothing is showing.
  void replace(gfx::Surface card);
  // Slide the current card away and show nothing.
  void dismiss();
  // Drop everything immediately, with no animation.
  void clear();
  // Swap the foreground surface in place without animating (content refresh).
  void set_current(gfx::Surface card);

  // Returns true if anything changed (caller should redraw).
  bool update();
  // Blit background (if any) at its resting y and the foreground at x; the
  // foreground's y comes from the slide animation.
  void render(gfx::Surface& dst, int x) const;

  bool idle() const;
  // a card is visible or animating
  bool on_screen() const;
  // foreground fully shown and not animating
  bool settled() const;
  bool animating() const;

  // Called once when the slider settles into the hidden state with no pending
  // card. Use for cleanup (e.g. clear selection).
  void on_hidden(std::function<void()> cb);

private:
  SlideAnimator anim_;
  int target_offset_;
  std::optional<gfx::Surface> foreground_;
  std::optional<gfx::Surface> background_;
  std::optional<gfx::Surface> pending_;
  std::function<void()> on_hidden_;
};

} // namespace views::ui
