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
#include "views/helpers/CardGfx.h"

PlayerTurnCard::PlayerTurnCard(GameContext& context, GameState& state,
                               PlayerTurnCardState ps)
    : MainPanelView(context, state), player_(ps.player), date_(ps.date),
      on_closed_(std::move(ps.on_closed)) {}

void PlayerTurnCard::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  ctx.surface->fill(240);
  gfx::blit::blit_with_palette(*ctx.surface, context_.vga->get("gfx/back39"));

  auto* player = state_.player(player_);
  auto name = player->name();
  const TextStyle style{
      .color = 24,
      .align = TextAnchor::Middle,
      .shadow = ShadowStyle{.offset = PxOffset{-2, 2}, .color = 20}};

  context_.font->render_text_onto(*ctx.surface, name,
                                  {ctx.surface->width() / 2, 60},
                                  FontFace::ASTANDAR, style);

  views::helpers::draw_framed_portrait(*ctx.surface, player->character(),
                                       {140, 100});

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
    if (on_closed_) {
      on_closed_();
    }
  }
}
