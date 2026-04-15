/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/layers/PauseLayer.h"
#include "core/state/GameState.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/AlertPopup.h"

PauseLayer::PauseLayer(GameContext& context, GameState& state)
    : View(context, state), listener_(context.input->register_listener(*this)) {
}

void PauseLayer::render(const RenderContext& ctx) {}

bool PauseLayer::on_action(const input::ActionEvent& ev) {
  if (ev.action == input::Action::TogglePause && ev.state.pressed) {
    if (!is_paused_) {
      mark_dirty();
      is_paused_ = true;
      state_.pause_time_advancement();
      context_.popup->origin(ViewPoint{100, 100});
      context_.popup->create<AlertPopup>("Pause!", [this]() {
        is_paused_ = false;
        state_.resume_time_advancement();
      });
      return true;
    }
  }
  return false;
}

void PauseLayer::tick() {}
