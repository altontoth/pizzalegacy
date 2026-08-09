/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/MaestroMessage.h"

#include <utility>

#include "core/state/GameState.h"
#include "engine/DisplayConstants.h"
#include "engine/ui/InteractionEvent.h"
#include "resources/Font.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"

namespace {
bool is_left_click(const views::ui::InteractionEvent& ie) {
  return ie.type == views::ui::InteractionEvent::Type::Click &&
         ie.button == views::ui::MouseButton::Left;
}
} // namespace

MaestroMessage::MaestroMessage(GameContext& context, GameState& state,
                               MaestroMessageState ms)
    : View(context, state), text_(std::move(ms.text)),
      on_dismiss_(std::move(ms.on_dismiss)) {

  interactions_.create_persistent_item(
      ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      "maestro_dismiss", [this](views::ui::InteractionEvent ie) {
        if (is_left_click(ie)) {
          state_.ui().pop();
          if (on_dismiss_) {
            on_dismiss_();
          }
        }
      });
}

void MaestroMessage::render(const RenderContext& ctx) {
  ctx.surface->fill(64);
  ctx.surface->palette({41});
  ctx.surface->blit_masked(context_.vga->get("gfx/maestro"), ViewPoint{0, 0});

  TextStyle style{
      .color = 240,
      .align = TextAnchor::Start,
      .shadow = std::nullopt,
      .character_spacing = std::nullopt,
      .line_spacing = 10,
  };

  TextStyle center_style{
      .color = 240,
      .align = TextAnchor::Middle,
      .shadow = std::nullopt,
      .character_spacing = std::nullopt,
      .line_spacing = 10,
  };

  // Lines below the portrait's shoulder start further right.
  constexpr int text_left = 300;
  constexpr int text_top = 24;
  constexpr int text_right = 600;
  constexpr int indent_below_y = 140;
  constexpr int indent = 66;

  FlowColumns flow{
      .cutoff_y = indent_below_y,
      .left_margin_above = text_left,
      .left_margin_below = text_left + indent,
      .right_margin = text_right,
  };

  context_.font->render_text_onto(*ctx.surface, text_,
                                  ViewPoint{text_left, text_top},
                                  FontFace::PIZ_NUMB, style, flow);

  // We have to go around the Maestro's hand which sticks out
  constexpr int prompt_x_offset = 85;

  context_.font->render_line_onto(
      *ctx.surface, "Click to continue",
      ViewPoint{(DisplayConstants::SCREEN_WIDTH / 2) + prompt_x_offset,
                DisplayConstants::SCREEN_HEIGHT - 30},
      FontFace::PIZ_NUMB, center_style);
}
