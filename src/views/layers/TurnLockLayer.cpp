/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/layers/TurnLockLayer.h"

#include <string_view>

#include "core/state/GameState.h"
#include "core/state/TurnOrchestrator.h"
#include "engine/DisplayConstants.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionManager.h"
#include "views/ui/BusyCursor.h"
#include "views/ui/Icon.h"

namespace {
constexpr Icon LOCK_ICON = Icon::PADLOCK;
constexpr std::string_view LOCK_ITEM = "turn_lock";
} // namespace

TurnLockLayer::TurnLockLayer(GameContext& context, GameState& state)
    : View(context, state, views::ui::priority_turn_lock) {}

void TurnLockLayer::render(const RenderContext& /*ctx*/) {}

void TurnLockLayer::tick() {
  bool lock = state_.turn().is_pipeline_blocking_input();
  if (lock == locked_) {
    return;
  }
  locked_ = lock;
  if (lock) {
    interactions_.create_persistent_item(
        ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
                 DisplayConstants::SCREEN_HEIGHT},
        LOCK_ITEM, [](const views::ui::InteractionEvent&) {},
        {.layer = views::ui::InteractionLayer::Exclusive});
    context_.cursor->custom_cursor(
        views::ui::icon_surface(*context_.vga, LOCK_ICON));
  }
  else {
    interactions_.clear_persistent(LOCK_ITEM);
    context_.cursor->clear_custom_cursor();
  }
}
