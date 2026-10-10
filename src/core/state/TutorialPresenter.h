/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>

namespace core::tutorial {
struct Step;
}

// Port for the tutorial's text panel and the frame around the target.
class TutorialPresenter {
public:
  virtual ~TutorialPresenter() = default;

  virtual void show_step(const core::tutorial::Step& step,
                         std::function<void()> on_dismissed) = 0;
  virtual void tutorial_ended() = 0;
};

class NoTutorialPresenter : public TutorialPresenter {
public:
  void show_step(const core::tutorial::Step& /*step*/,
                 std::function<void()> on_dismissed) override {
    if (on_dismissed) {
      on_dismissed();
    }
  }
  void tutorial_ended() override {}
};
