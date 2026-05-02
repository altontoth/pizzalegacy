/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/PizzaJudges.h"
#include "core/state/GameState.h"
#include "engine/DisplayConstants.h"
#include "views/base/ViewDefs.h"

PizzaJudges::PizzaJudges(GameContext& context, GameState& state,
                         const std::array<int, NUMBER_OF_JUDGES>& scores,
                         std::function<void()> on_complete,
                         JudgingMode judging_mode)
    : View(context, state), animator_(context), on_complete_(on_complete) {
  animator_.start(scores, judging_mode);
}

void PizzaJudges::register_interactions() {
  interactions_.create_frame_item(
      ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      "pizzajudges_root", [this](const views::ui::InteractionEvent& ie) {
        if (ie.type != views::ui::InteractionEvent::Type::Click ||
            ie.button != views::ui::MouseButton::Left) {
          return;
        }

        if (!animator_.is_complete()) {
          // Skip to end of animation
          animator_.skip_to_end();
          mark_dirty();
        }
        else {
          // Animation complete, call completion callback
          if (on_complete_) {
            on_complete_();
          }
        }
      });
}

void PizzaJudges::tick() {
  if (animator_.tick()) {
    mark_dirty();
  }
}

void PizzaJudges::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  animator_.render(ctx);
  register_interactions();
}
