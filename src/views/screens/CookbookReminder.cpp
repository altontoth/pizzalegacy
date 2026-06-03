/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/CookbookReminder.h"

#include "core/state/GameState.h"
#include "engine/DisplayConstants.h"
#include "engine/FormatCompat.h"
#include "engine/ui/InteractionEvent.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"

namespace {
bool is_left_click(const views::ui::InteractionEvent& ie) {
  return ie.type == views::ui::InteractionEvent::Type::Click &&
         ie.button == views::ui::MouseButton::Left;
}
} // namespace

CookbookReminder::CookbookReminder(GameContext& context, GameState& state,
                                   CookbookReminderState crs)
    : View(context, state), page_(crs.cookbook_page) {

  interactions_.create_persistent_item(
      ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      "practice_dismiss", [this](views::ui::InteractionEvent ie) {
        if (is_left_click(ie)) {
          state_.ui().pop();
        }
      });
}

void CookbookReminder::render(const RenderContext& ctx) {
  ctx.surface->fill(64);
  ctx.surface->palette({41});
  ctx.surface->blit_masked(context_.vga->get("gfx/maestro"), ViewPoint{0, 0});

  TextStyle style{
      .color = 240,
      .align = AlignX::Left,
      .shadow = std::nullopt,
      .character_spacing = std::nullopt,
      .line_spacing = 10,
  };

  TextStyle center_style{
      .color = 240,
      .align = AlignX::CenterPoint,
      .shadow = std::nullopt,
      .character_spacing = std::nullopt,
      .line_spacing = 10,
  };

  std::string text = compat::format(
      "You need a bit more practice baking the standard recipes. This recipe "
      "can be found on page {} of the great Pizza Tycoon cook book.",
      page_);

  context_.font->render_into_rect(*ctx.surface, text,
                                  ViewRect{300, 44, 300, 140},
                                  FontFace::PIZ_NUMB, style);

  context_.font->render_line_onto(
      *ctx.surface, "Click to continue",
      ViewPoint{DisplayConstants::SCREEN_WIDTH / 2,
                DisplayConstants::SCREEN_HEIGHT - 30},
      FontFace::PIZ_NUMB, center_style);
}
