/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/GameOver.h"

#include "core/state/GameState.h"
#include "gfx/Blit.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"
#include "views/base/RawRenderer.h"

GameOver::GameOver(GameContext& context, GameState& state, GameOverState gos)
    : View(context, state), player_(state.player(gos.player_id)) {}

void GameOver::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  ctx.surface->fill(64);
  ctx.surface->palette({41});
  ctx.surface->blit_masked(context_.vga->get("gfx/maestro"), ViewPoint{0, 0});

  auto has_high_score = false;
  std::string score_text;
  TextStyle style{
      .color = 240,
      .align = AlignX::Left,
      .shadow = std::nullopt,
      .character_spacing = std::nullopt,
      .line_spacing = 10,
  };

  if (has_high_score) {
    score_text = compat::format(
        "{}, you have achieved a high score! Please type in the name under "
        "which the score will be stored in the high score table.",
        player_->name());
  }
  else {
    score_text = "Game over! Sorry, it was not good enough to go in the high "
                 "score table - maybe next time!";
  }

  context_.font->render_into_rect(*ctx.surface, score_text,
                                  ViewRect{300, 44, 300, 140},
                                  FontFace::PIZ_NUMB, style);
}
