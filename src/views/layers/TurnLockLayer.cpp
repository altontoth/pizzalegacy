/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/layers/TurnLockLayer.h"

#include "core/state/GameState.h"
#include "core/state/TurnOrchestrator.h"
#include "engine/DisplayConstants.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionManager.h"
#include "views/ui/BusyCursor.h"
#include "views/ui/Icon.h"

namespace {
constexpr Icon LOCK_ICON = Icon::PADLOCK;
} // namespace

TurnLockLayer::TurnLockLayer(GameContext& context, GameState& state)
    : View(context, state) {}

// The lock item is recreated every frame: views like the city map register
// their interaction items per frame too, and only a newer item sits above
// them. Rendering after the view and before PopupManager keeps the lock
// above the view's items and popups above the lock.
void TurnLockLayer::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  if (!locked_) {
    return;
  }
  interactions_.create_frame_item(
      ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      "turn_lock", [](const views::ui::InteractionEvent&) {}, {},
      views::ui::InteractionLayer::Exclusive);
}

void TurnLockLayer::tick() {
  bool lock = state_.turn().is_pipeline_blocking_input();
  if (lock != locked_) {
    locked_ = lock;
    // Unlocking needs a render too: only render() drops the lock item (TODO:
    // move the locking from the render method by introducing a proper
    // priority system and can use a persistent item instead).
    mark_dirty();
    if (lock) {
      context_.cursor->custom_cursor(
          views::ui::icon_surface(*context_.vga, LOCK_ICON));
    }
    else {
      context_.cursor->clear_custom_cursor();
    }
  }
  if (locked_) {
    mark_dirty();
  }
}
