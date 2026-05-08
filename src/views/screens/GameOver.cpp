/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/GameOver.h"

#include "core/character/Player.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "engine/DisplayConstants.h"
#include "engine/FormatCompat.h"
#include "engine/input/InputManager.h"
#include "engine/ui/InteractionEvent.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"

constexpr int high_score_threshold = 50000;

namespace {
bool is_left_click(const views::ui::InteractionEvent& ie) {
  return ie.type == views::ui::InteractionEvent::Type::Click &&
         ie.button == views::ui::MouseButton::Left;
}
} // namespace

GameOver::GameOver(GameContext& context, GameState& state, GameOverState gos)
    : View(context, state), player_(state.player(gos.player_id)),
      cause_(gos.cause),
      phase_(cause_ == GameOverCause::WorldDomination ? Phase::ContinuePrompt
                                                      : Phase::ScoreScreen),
      listener_(context_.input->register_listener(*this)) {

  constexpr int btn_y = 300;
  constexpr int yes_x = 180;
  constexpr int no_x = 380;
  constexpr int btn_w = 120;
  constexpr int btn_h = 40;

  interactions_.create_persistent_item(
      ViewRect{yes_x, btn_y, btn_w, btn_h}, "gameover_yes",
      [this](views::ui::InteractionEvent ie) {
        if (phase_ == Phase::ContinuePrompt && is_left_click(ie)) {
          continue_playing();
        }
      });

  interactions_.create_persistent_item(
      ViewRect{no_x, btn_y, btn_w, btn_h}, "gameover_no",
      [this](views::ui::InteractionEvent ie) {
        if (phase_ == Phase::ContinuePrompt && is_left_click(ie)) {
          phase_ = Phase::ScoreScreen;
        }
      });

  interactions_.create_persistent_item(
      ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      "gameover_score", [this](views::ui::InteractionEvent ie) {
        if (phase_ == Phase::ScoreScreen && is_left_click(ie)) {
          go_to_main_menu();
        }
      });
}

bool GameOver::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }
  if (phase_ == Phase::ScoreScreen) {
    go_to_main_menu();
    return true;
  }
  return false;
}

void GameOver::continue_playing() {
  if (player_) {
    player_->restore_active();
  }
  state_.ui().pop();
}

void GameOver::go_to_main_menu() {
  state_.ui().pop_to_root();
  state_.ui().push<ScreenTag>(ScreenTag::Kind::MenuMain);
}

void GameOver::render(const RenderContext& ctx) {
  interactions_.begin_frame();
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
      .align = AlignX::Center,
      .shadow = std::nullopt,
      .character_spacing = std::nullopt,
      .line_spacing = 10,
  };

  if (phase_ == Phase::ContinuePrompt) {
    context_.font->render_into_rect(
        *ctx.surface,
        "Congratulations! You have achieved world domination! "
        "Would you like to continue playing?",
        ViewRect{100, 80, 440, 180}, FontFace::PIZ_NUMB, style);

    context_.font->render_line_onto(*ctx.surface, "Yes - keep playing",
                                    ViewPoint{240, 318}, FontFace::PIZ_NUMB,
                                    center_style);
    context_.font->render_line_onto(*ctx.surface, "No - end game",
                                    ViewPoint{440, 318}, FontFace::PIZ_NUMB,
                                    center_style);
    return;
  }

  bool has_high_score = player_ && player_->score() >= high_score_threshold;
  std::string score_text;
  if (has_high_score) {
    score_text = compat::format(
        "{}, you have achieved a high score! Please type in the name under "
        "which the score will be stored in the high score table.",
        player_->name());
    // TODO: name entry text input
  }
  else {
    score_text = "Game over! Sorry, it was not good enough to go in the high "
                 "score table - maybe next time!";
  }

  context_.font->render_into_rect(*ctx.surface, score_text,
                                  ViewRect{300, 44, 300, 140},
                                  FontFace::PIZ_NUMB, style);

  context_.font->render_line_onto(
      *ctx.surface, "Click to return to main menu",
      ViewPoint{DisplayConstants::SCREEN_WIDTH / 2,
                DisplayConstants::SCREEN_HEIGHT - 30},
      FontFace::PIZ_NUMB, center_style);
}
