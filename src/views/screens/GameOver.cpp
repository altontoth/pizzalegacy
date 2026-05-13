/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/GameOver.h"

#include "core/character/Player.h"
#include "core/save/SaveGameManager.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "engine/DisplayConstants.h"
#include "engine/FormatCompat.h"
#include "engine/ui/InteractionEvent.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/ui/popups/TextInputPopup.h"

constexpr int high_score_threshold = 50'000;
constexpr int name_max_characters = 20;

namespace {
bool is_left_click(const views::ui::InteractionEvent& ie) {
  return ie.type == views::ui::InteractionEvent::Type::Click &&
         ie.button == views::ui::MouseButton::Left;
}
} // namespace

GameOver::GameOver(GameContext& context, GameState& state, GameOverState gos)
    : View(context, state), player_(state.player(gos.player_id)),
      cause_(gos.cause),
      screen_(player_ && player_->score() >= high_score_threshold
                  ? GameOverScreen::HighScore
                  : GameOverScreen::NoHighScore) {

  interactions_.create_persistent_item(
      ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      "gameover_dismiss", [this](views::ui::InteractionEvent ie) {
        if (popup_pending_ && is_left_click(ie)) {
          popup_pending_ = false;
          start_flow();
        }
      });
}

void GameOver::start_flow() {
  if (screen_ == GameOverScreen::HighScore) {
    context_.popup->create<TextInputPopup>(
        "Enter name for the high score table:", name_max_characters,
        [this](std::string name) {
          if (name.empty() && player_) {
            name = player_->name();
          }
          save::SaveGameManager::add_high_score(save::HighScoreEntry{
              .name = std::move(name),
              .score = player_ ? player_->score() : 0,
          });
          after_high_score();
        });
  }
  else {
    after_high_score();
  }
}

void GameOver::after_high_score() {
  if (cause_ == GameOverCause::PizzaTycoon) {
    context_.popup->create<ConfirmPopup>("Would you like to continue playing?",
                                         [this](bool yes) {
                                           if (yes) {
                                             continue_playing();
                                           }
                                           else {
                                             go_to_main_menu();
                                           }
                                         });
  }
  else {
    go_to_main_menu();
  }
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
      .align = AlignX::CenterPoint,
      .shadow = std::nullopt,
      .character_spacing = std::nullopt,
      .line_spacing = 10,
  };

  std::string text;
  if (screen_ == GameOverScreen::HighScore) {
    text = compat::format("{}, you have achieved a high score!",
                          player_ ? player_->name() : "Player");
  }
  else {
    text = "Game over! Sorry, it was not good enough to go in the high "
           "score table - maybe next time!";
  }

  context_.font->render_into_rect(*ctx.surface, text,
                                  ViewRect{300, 44, 300, 140},
                                  FontFace::PIZ_NUMB, style);

  context_.font->render_line_onto(
      *ctx.surface, "Click to continue",
      ViewPoint{DisplayConstants::SCREEN_WIDTH / 2,
                DisplayConstants::SCREEN_HEIGHT - 30},
      FontFace::PIZ_NUMB, center_style);
}
