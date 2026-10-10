/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/tutorial/Tutorial.h"

#include <functional>
#include <string>
#include <variant>

#include "core/state/GameState.h"
#include "core/state/TutorialPresenter.h"
#include "core/tutorial/TutorialSteps.h"
#include "engine/Logger.h"

namespace core::tutorial {

void Tutorial::start() { enter(0); }

void Tutorial::finish() {
  if (!step_) {
    return;
  }
  Logger::info("Tutorial left at step {}", *step_);
  step_.reset();
  gs_->tutorial_presenter().tutorial_ended();
}

const Step* Tutorial::current_step() const {
  if (!step_) {
    return nullptr;
  }
  return &tutorial_steps()[*step_];
}

bool Tutorial::on_last_step() const {
  return step_ && *step_ + 1 == step_count();
}

std::size_t Tutorial::step_count() { return tutorial_steps().size(); }

void Tutorial::advance() {
  if (!step_) {
    return;
  }
  if (on_last_step()) {
    finish();
    return;
  }
  enter(*step_ + 1);
}

void Tutorial::item_clicked(std::string_view name) {
  const Step* step = current_step();
  if (!step || step->completion != Completion::TargetClicked || !step->target) {
    return;
  }
  const auto* item = std::get_if<std::string>(&*step->target);
  if (item && *item == name) {
    advance();
  }
}

void Tutorial::tick() {
  const Step* step = current_step();
  if (!step || step->completion != Completion::Condition || !step->condition) {
    return;
  }
  if (!step->condition(*gs_)) {
    steady_ticks_ = 0;
    return;
  }
  if (++steady_ticks_ > step->steady_ticks) {
    advance();
  }
}

void Tutorial::present() {
  const Step* step = current_step();
  if (!step) {
    return;
  }
  const uint32_t shown = ++show_count_;
  std::function<void()> on_dismissed;
  if (step->completion == Completion::Dismissed) {
    on_dismissed = [this, shown] {
      if (show_count_ == shown) {
        advance();
      }
    };
  }
  gs_->tutorial_presenter().show_step(*step, std::move(on_dismissed));
}

void Tutorial::enter(std::size_t index) {
  const Step& step = tutorial_steps()[index];
  if (step.skip_if && step.skip_if(*gs_) && index + 1 < step_count()) {
    Logger::info("Tutorial step {} passed over: {}", index, step.name);
    enter(index + 1);
    return;
  }
  step_ = index;
  steady_ticks_ = 0;
  Logger::info("Tutorial step {}: {}", index, step.name);
  if (step.on_enter) {
    step.on_enter(*gs_);
  }
  present();
}

} // namespace core::tutorial
