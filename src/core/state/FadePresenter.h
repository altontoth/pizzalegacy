/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>

// Port for the fade that takes us through the end of day fade so we can do it
// without having a dependency on the Renderer.
class FadePresenter {
public:
  virtual ~FadePresenter() = default;

  // Fades out, runs on_midpoint while the screen is faded out, then fades back
  // in. Returns false when there is no view to fade, in which case on_midpoint
  // is not run.
  virtual bool fade_through_black(int duration_frames,
                                  std::function<void()> on_midpoint) = 0;
};

class NoFadePresenter : public FadePresenter {
public:
  bool fade_through_black(int /*duration_frames*/,
                          std::function<void()> /*on_midpoint*/) override {
    return false;
  }
};
