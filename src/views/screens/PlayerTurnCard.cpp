/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/PlayerTurnCard.h"

#include "core/state/GameState.h"
#include "gfx/Blit.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"
#include "views/base/RawRenderer.h"

PlayerTurnCard::PlayerTurnCard(GameContext& context, GameState& state,
                               PlayerTurnCardState ps)
    : MainPanelView(context, state), player_(ps.player), date_(ps.date) {}

void PlayerTurnCard::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  ctx.surface->fill(240);
  gfx::blit::blit_with_palette(*ctx.surface, context_.vga->get("gfx/back39"));

  auto* player = state_.player(player_);
  RawRenderer r;
  auto picture = r.surface(player->character());
  auto name = player->name();
  const TextStyle style{
      .color = 24,
      .align = AlignX::Center,
      .shadow = ShadowStyle{.offset = PxOffset{-2, 2}, .color = 20}};

  context_.font->render_text_onto(*ctx.surface, name, {0, 60},
                                  FontFace::ASTANDAR, style);

  // Photo frame background colors
  uint8_t photo_white_bg = 15;
  uint8_t photo_darker_bg = 11;
  uint8_t photo_light_bg = 12;
  ViewRect photo_location(140, 100, 118, 132);

  // Local lambda to draw a frame layer
  auto draw_frame_layer = [ctx](const ViewRect& base, int dx, int dy,
                                uint8_t color) {
    ViewRect r{base.x + dx, base.y + dy, base.w, base.h};
    ctx.surface->fill_rect(r, color);
  };

  // Draw frame from back to front
  draw_frame_layer(photo_location, 0, 0, photo_light_bg);
  draw_frame_layer(photo_location, 2, -2, photo_darker_bg);
  draw_frame_layer(photo_location, 4, -4, photo_white_bg);

  ctx.surface->blit_raw(picture,
                        ViewPoint{photo_location.x + 11, photo_location.y + 3});

  context_.font->render_text_onto(*ctx.surface,
                                  std::to_string(date_.day()) + ".", {90, 246},
                                  FontFace::MENU, 25);
  context_.font->render_text_onto(*ctx.surface, date_.month_name_short(),
                                  {164, 246}, FontFace::MENU, 240);
  context_.font->render_text_onto(*ctx.surface,
                                  std::to_string(date_.year_short()),
                                  {266, 246}, FontFace::MENU, 240);
}

void PlayerTurnCard::tick() {
  if (delay_ticks_-- == 0) {
    state_.ui().pop();
    state_.on_player_turn_card_finished();
  }
}
