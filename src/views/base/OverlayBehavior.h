/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <utility>
#include "engine/DebugLabel.h"
#include "engine/Logger.h"

struct IOverlay {
  virtual ~IOverlay() = default;
  virtual bool is_finished() const = 0;
  virtual bool pull_recompose_flag() const = 0;
  // Veto the main view's tick() (input popups, console, pause).
  virtual bool freezes_view() const = 0;
  // Veto GameState::update() entirely: no clock, no end-of-turn pipeline.
  virtual bool freezes_simulation() const { return false; }
};

template <typename Derived> class OverlayBehavior : public IOverlay {
protected:
  void show() {
    Logger::debug("Showing overlay {}", debug::label_of(this));
    active_ = true;
    mark_self_dirty();
    request_recompose();
  }
  void hide() {
    Logger::debug("Hiding overlay {}", debug::label_of(this));
    active_ = false;
    mark_self_dirty();
    request_recompose();
  }
  bool is_active() const { return active_; }
  bool is_finished() const override { return finished_; }

  // tell renderer to re-blit everything once
  void request_recompose() const { force_recompose_ = true; }

  bool pull_recompose_flag() const override {
    // one-shot flag: true once, then clears itself
    return std::exchange(force_recompose_, false);
  }

  void finish() {
    finished_ = true;
    hide();
  }
  void mark_self_dirty() { static_cast<Derived*>(this)->mark_dirty(); }
  bool freezes_view() const override { return freezes_view_; }
  void freezes_view(bool freezes) { freezes_view_ = freezes; }

private:
  mutable bool force_recompose_ = false;
  // Is this currently painting something to the screen?
  bool active_ = false;
  // Should this be removed?
  bool finished_ = false;
  // Does this block the main view from processing logic?
  bool freezes_view_ = false;
};
