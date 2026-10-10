/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// The pizza competition flow: ingredient reveal, timed design, AI simulation,
// prize distribution and the results table.

#include "core/GameTypes.h"
#include "core/food/PizzaJudging.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"
#include "engine/audio/AudioManager.h"
#include "engine/gfx/Blit.h"
#include "engine/ui/TextAnchor.h"
#include "views/screens/pizza/PizzaDesign.h"
#include "views/screens/pizza/PizzaDesignHelpers.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/AlertPopup.h"

#include <algorithm>
#include <memory>
#include <string>

static constexpr uint64_t GET_READY_WAIT_MS = 2000;
static constexpr uint64_t RESULTS_ROW_INTERVAL_MS = 1200;

using namespace core::food;
using namespace pizza_design;

Player* PizzaDesign::competing_player() const {
  if (competing_player_id_) {
    return state_.player(*competing_player_id_);
  }
  return state_.active_player();
}

void PizzaDesign::begin_competition(std::shared_ptr<CompetitionSession> s) {
  if (!context_.audio->is_playing()) {
    context_.audio->play_music(audio::MusicId::BREAK_IN_MUSIC_3);
  }
  competition_ = std::move(*s);
  reveal_delay_remaining_ticks_ = competition_->delay_ticks;

  update_competition_status();
  change_mode(PizzaDesignMode::CompetitionReveal);
}

void PizzaDesign::begin_annual_competition(
    std::shared_ptr<CompetitionSession> s) {
  competition_ = std::move(*s);
  bake_session(std::make_shared<core::food::BakeSession>());
  bake_->kind = BakeKind::NewUser;
  bake_->working = Pizza("annual competition pizza");
  bake_->working.update_stats(state_.customer_taste_profiles());
  design_mode_ = true;
  status_.show_pizza(PizzaCtx{
      .state = &state_,
      .wip_pizza = &bake_->working,
      .recipe = nullptr,
  });
  change_mode(PizzaDesignMode::AnnualCompetitionDesign);

  auto* player = competing_player();
  std::string first_name = player->character().first_name();
  context_.popup->create<AlertPopup>(
      compat::format("Right, {}! Let's see what you can do!", first_name),
      [this] { competition_design_ticks_ = 0; });
}

bool PizzaDesign::tick_competition() {
  if (mode_ == PizzaDesignMode::CompetitionReveal && competition_) {
    auto& c = *competition_;
    if (!c.reveal_done) {
      int topping_count =
          static_cast<int>(c.reference_pizza->toppings().size());
      if (c.reveal_idx < topping_count) {
        int current_max = reveal_speed_boost_ ? 1 : c.delay_ticks;
        reveal_delay_remaining_ticks_ =
            std::min(reveal_delay_remaining_ticks_, current_max);
        if (reveal_delay_remaining_ticks_ > 0) {
          --reveal_delay_remaining_ticks_;
        }
        else {
          reveal_delay_remaining_ticks_ = current_max;
          c.reveal_idx++;
          update_competition_status();
          mark_dirty();
        }
      }
      else {
        c.reveal_done = true;
        c.get_ready = true;
        c.get_ready_start_ms = context_.clock.now_ms;
        update_competition_status();
        mark_dirty();
      }
    }
    else if (c.get_ready) {
      if (context_.clock.elapsed_since(c.get_ready_start_ms,
                                       GET_READY_WAIT_MS)) {
        c.get_ready = false;
        on_reveal_complete();
      }
    }
    return true;
  }

  if (mode_ == PizzaDesignMode::CompetitionDesign) {
    competition_design_ticks_++;
  }

  if (mode_ == PizzaDesignMode::CompetitionResults && competition_) {
    auto& c = *competition_;
    int total = static_cast<int>(c.participants.size());
    if (c.results_rows_shown <= total) {
      if (context_.clock.elapsed_since(c.results_last_row_ms,
                                       RESULTS_ROW_INTERVAL_MS)) {
        c.results_rows_shown++;
        c.results_last_row_ms = context_.clock.now_ms;
        mark_dirty();
      }
    }
    return true;
  }

  return false;
}

void PizzaDesign::update_competition_status() {
  if (!competition_) {
    return;
  }
  auto& c = *competition_;
  std::string ingredient_name;
  if (!c.reveal_done) {
    int idx = c.reveal_idx - 1;
    if (idx >= 0 &&
        idx < static_cast<int>(c.reference_pizza->toppings().size())) {
      ingredient_name =
          c.reference_pizza->toppings()[idx]->ingredient()->name();
    }
  }
  status_.show_competition(CompetitionCtx{
      .pizza_name = c.reference_pizza->name(),
      .speed_level = c.speed_level,
      .current_ingredient = ingredient_name,
  });
}

void PizzaDesign::on_reveal_complete() {
  if (!competition_) {
    return;
  }

  // Set up empty bake session and switch to design mode BEFORE showing the
  // popup so the background already shows an empty pizza canvas.
  bake_session(std::make_shared<core::food::BakeSession>());
  bake_->kind = BakeKind::NewUser;
  bake_->working = Pizza("competition pizza");
  bake_->working.update_stats(state_.customer_taste_profiles());
  design_mode_ = true;
  status_.show_pizza(PizzaCtx{
      .state = &state_,
      .wip_pizza = &bake_->working,
      .recipe = nullptr,
  });
  change_mode(PizzaDesignMode::CompetitionDesign);

  auto* player = competing_player();
  std::string first_name = player->character().first_name();
  context_.popup->create<AlertPopup>(
      compat::format("Right, {}! Let's see what you can do!", first_name),
      [this] { competition_design_ticks_ = 0; });
}

void PizzaDesign::on_competition_design_finished() {
  if (!competition_ || !bake_) {
    return;
  }
  state_.advance_time_blocks(5);
  bake_->working.finalize_against(
      *competition_->reference_pizza, PizzaJudging::Original,
      state_.customer_taste_profiles(), "competition pizza");
  auto imitation_scores = bake_->working.scores();

  auto bake_ptr = bake_;
  bake_session(nullptr);
  competition_->human_design_ticks = competition_design_ticks_;
  auto comp_ptr =
      std::make_shared<CompetitionSession>(std::move(*competition_));
  competition_.reset();
  auto done = std::move(on_done_);

  state_.ui().push<PizzaJudgesState>(
      imitation_scores,
      [&state = state_, bake_ptr, comp_ptr, done = std::move(done)]() mutable {
        state.ui().pop();
        state.ui().replace_top<PizzaDesignState>(PizzaDesignState{
            .initial_mode = PizzaDesignMode::CompetitionJudging,
            .bake = bake_ptr,
            .competition_session = comp_ptr,
            .on_done = std::move(done),
        });
      },
      JudgingMode::Competition);
}

void PizzaDesign::finalize_competition_results() {
  if (!competition_) {
    return;
  }
  auto& c = *competition_;

  // Scoring, winner determination and bet settlement are game logic and live
  // in the coordinator; the returned winner event is delivered when the
  // results screen is dismissed, don't want to spoil the results :)
  pending_competition_message_ =
      state_.competitions().finalize_competition_results(c);

  c.results_rows_shown = 0;
  c.results_last_row_ms = context_.clock.now_ms;
  if (auto* rest = state_.active_restaurant()) {
    status_.show_restaurant(
        {.state = &state_, .rest = rest, .label = "COMPETITION"});
  }
  change_mode(PizzaDesignMode::CompetitionResults);
}

void PizzaDesign::draw_competition_reveal(const RenderContext& ctx) const {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back1"));
  ctx.surface->blit_masked(chop_horizontal_ ? blade_horizontal_
                                            : blade_vertical_,
                           ViewPoint{CHOPPER_OFFSET_X, CHOPPER_OFFSET_Y});

  if (!competition_) {
    return;
  }
  const auto& toppings = competition_->reference_pizza->toppings();
  int to_draw =
      std::min(competition_->reveal_idx, static_cast<int>(toppings.size()));
  for (int i = 0; i < to_draw; ++i) {
    renderer_.render(*toppings[i], *ctx.surface, camera_);
  }
}

void PizzaDesign::draw_competition_results(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back4"));
  if (!competition_) {
    return;
  }
  auto& c = *competition_;

  constexpr int text_color = 6;
  context_.font->render_text_onto(
      *ctx.surface, "PIZZA COMPETITION",
      ViewPoint{(ctx.surface->width() / 2) - 20, 42}, FontFace::ASTANDAR,
      TextStyle{.color = text_color, .align = TextAnchor::Middle});

  constexpr int col_name = 46;
  constexpr int col_time = 234;
  constexpr int col_match = 286;
  constexpr int col_final = 322;
  constexpr int row_height = 18;
  int y = 80;

  int rows_to_draw =
      std::min(c.results_rows_shown, static_cast<int>(c.participants.size()));

  for (int i = 0; i < rows_to_draw; ++i) {
    const auto& p = c.participants[i];
    auto* player_obj = state_.player(p.player_id);
    std::string name = player_obj ? player_obj->name() : "?";
    int m = p.time_ticks / 60;
    int s = p.time_ticks % 60;
    std::string time_str = compat::format("{}:{:02}", m, s);
    std::string match_str = compat::format("{} %", p.match_score);
    std::string final_str = std::to_string(p.final_score);

    context_.font->render_text_onto(
        *ctx.surface, name, ViewPoint{col_name, y}, FontFace::PIZ_NUMB,
        TextStyle{.color = text_color, .character_spacing = 1});
    context_.font->render_text_onto(*ctx.surface, time_str,
                                    ViewPoint{col_time, y}, FontFace::PIZ_NUMB,
                                    text_color, TextAnchor::End);
    context_.font->render_text_onto(*ctx.surface, match_str,
                                    ViewPoint{col_match, y}, FontFace::PIZ_NUMB,
                                    text_color, TextAnchor::End);
    context_.font->render_text_onto(*ctx.surface, final_str,
                                    ViewPoint{col_final, y}, FontFace::PIZ_NUMB,
                                    text_color, TextAnchor::End);
    y += row_height;
  }

  // Once all participant rows are visible we show the winner/draw:
  if (c.results_rows_shown > static_cast<int>(c.participants.size())) {
    std::string winner_text;
    if (c.winner_slot < 0 || c.all_tied) {
      winner_text = "-- draw --";
    }
    else {
      auto* winner_player =
          state_.player(c.participants[c.winner_slot].player_id);
      std::string wname =
          winner_player ? winner_player->name() : "unknown player";
      winner_text = wname + " wins";
    }
    y = 285;
    context_.font->render_text_onto(*ctx.surface, winner_text,
                                    ViewPoint{col_name, y}, FontFace::ASTANDAR,
                                    text_color);
  }
}
