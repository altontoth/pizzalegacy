/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/StatusDisplay.h"

#include <algorithm>

#include "core/character/Player.h"
#include "core/state/GameState.h"
#include "engine/geom/Spaces.h"
#include "resources/FontManager.h"
#include "views/base/ViewDefs.h"

StatusDisplay::StatusDisplay(GameContext& context, GameState& state,
                             StatusHub& hub)
    : PanelView(context, state,
                ViewPoint{views::base::STATUS_PANEL_OFFSET_X,
                          views::base::STATUS_PANEL_OFFSET_Y}),
      hub_(hub) {

  sub_ =
      hub_.subscribe([this](StatusDisplayRecipe r) { recipe(std::move(r)); });
}

void StatusDisplay::tick() {
  // Re-register every frame: I used persistent frame in the past but
  // interaction_manager_->clear() is called on every UI transition, so
  // constructor-registered items don't survive screen changes (because status
  // display now lives on in GameState even when not active).
  interactions_.begin_frame();
  interactions_.create_frame_item(
      ViewRect{0, 0, views::base::STATUS_PANEL_WIDTH,
               views::base::STATUS_PANEL_HEIGHT},
      "status_panel", [this](views::ui::InteractionEvent ie) {
        if (ie.type != views::ui::InteractionEvent::Type::Click) {
          return;
        }
        if (ie.button == views::ui::MouseButton::Left) {
          state_.status().show_baseline();
        }
        else if (ie.button == views::ui::MouseButton::Right) {
          state_.status().show_previous();
        }
        mark_dirty();
      });

  const auto blocks = state_.time().time_blocks();
  if (previous_time_ != blocks) {
    previous_time_ = blocks;
    mark_dirty();
  }

  const Player* money_player = state_.active_player();
  if (hub_.header_frozen()) {
    money_player =
        hub_.frozen_player() ? state_.player(*hub_.frozen_player()) : nullptr;
  }
  if (money_player) {
    const auto balance = money_player->cash_balance();
    if (previous_balance_ != balance) {
      previous_balance_ = balance;
      mark_dirty();
    }
  }
}

void StatusDisplay::render(const RenderContext& ctx) {
  clear(ctx);

  if (attack_mode_) {
    render_attack_bars(ctx);
    return;
  }

  // Render fixed elements (date, time, money)
  render_header_info(ctx);

  // Render the dynamic recipe content
  constexpr int content_start_y = 16;
  render_recipe(recipe_, ctx);
}

void StatusDisplay::render_recipe(const StatusDisplayRecipe& recipe,
                                  const RenderContext& ctx) {
  auto to_alignment = [](StatusAlignment a) {
    switch (a) {
    case StatusAlignment::Left:
      return TextAnchor::Start;
    case StatusAlignment::Center:
      return TextAnchor::Middle;
    case StatusAlignment::Right:
      return TextAnchor::End;
    }
    return TextAnchor::Start;
  };
  // A centered or right-aligned column gives its x relative to the panel's
  // center or right edge.
  auto to_x = [&ctx](const StatusColumn& col) {
    switch (col.align) {
    case StatusAlignment::Left:
      return col.x;
    case StatusAlignment::Center:
      return (ctx.surface->width() / 2) + col.x;
    case StatusAlignment::Right:
      return ctx.surface->width() + col.x;
    }
    return col.x;
  };
  int y = 16;
  const int color_yellow = 16;
  const int color_white = 14;
  for (const auto& item : recipe.items()) {
    int c = (item.color != 0) ? item.color : color_white;
    switch (item.kind) {
    case StatusItemKind::Header:
      context_.font->render_text_onto(*ctx.surface, item.col1.text,
                                      ViewPoint{item.col1.x, y},
                                      FontFace::SUPERSMA, c);
      y += 12;
      break;
    case StatusItemKind::TwoColumn: {
      const auto& col1 = item.col1;
      const auto& col2 = item.col2;
      context_.font->render_text_onto(
          *ctx.surface, col1.text, ViewPoint{to_x(col1), y}, FontFace::SUPERSMA,
          c, to_alignment(col1.align));
      context_.font->render_text_onto(
          *ctx.surface, col2.text, ViewPoint{to_x(col2), y}, FontFace::SUPERSMA,
          c, to_alignment(col2.align));
      y += 12;
      break;
    }
    case StatusItemKind::Single:
      context_.font->render_line_onto(
          *ctx.surface, item.col1.text, ViewPoint{to_x(item.col1), y},
          FontFace::SUPERSMA, c, to_alignment(item.col1.align));
      y += 12;
      break;
    case StatusItemKind::Spacer:
      y += item.lines * 12;
      break;
    }
  }
}

void StatusDisplay::render_header_info(const RenderContext& ctx) {
  constexpr int date_x_offset = 15;
  constexpr int time_x_offset = 110;
  constexpr int date_time_y_offset = 2;
  constexpr int text_color = 16; // yellow

  // Frozen header reads the snapshot and the frozen player's live balance;
  // otherwise it tracks GameState and the active player.
  std::string date_str, time_str, money_str;
  if (hub_.header_frozen()) {
    const GameTime& time = hub_.frozen_time();
    date_str = time.to_date_string();
    time_str = time.to_time_string();
    if (auto id = hub_.frozen_player()) {
      if (auto* player = state_.player(*id)) {
        money_str = compat::format("{}", player->cash_balance());
      }
    }
  }
  else {
    const GameTime& time = state_.time();
    date_str = time.to_date_string();
    time_str = time.to_time_string();

    // For debug reasons, sometimes we get into places without active player
    // thanks to the console, so leave this for now:
    if (state_.active_player()) {
      money_str = compat::format("{}", state_.active_player()->cash_balance());
    }
  }

  context_.font->render_text_onto(*ctx.surface, date_str,
                                  ViewPoint{date_x_offset, date_time_y_offset},
                                  FontFace::SUPERSMA, text_color);
  context_.font->render_text_onto(*ctx.surface, time_str,
                                  ViewPoint{time_x_offset, date_time_y_offset},
                                  FontFace::SUPERSMA, text_color);

  if (!money_str.empty()) {
    constexpr int money_x_offset = 60;
    constexpr int money_y_offset = 164;
    TextStyle ts{
        .color = text_color,
        .align = TextAnchor::Start,
        .shadow = std::nullopt,
        .character_spacing = 2,
    };
    context_.font->render_line_onto(*ctx.surface, money_str,
                                    ViewPoint{money_x_offset, money_y_offset},
                                    FontFace::SUPERSMA, ts);
  }
}

void StatusDisplay::render_attack_bars(const RenderContext& ctx) {
  constexpr int bar_w = 50;
  constexpr int bar_max_h = 140;
  constexpr int bar_bottom = 160;
  constexpr int attacker_x = 30;
  constexpr int defender_x = 90;
  constexpr int empty_color = 240;
  constexpr int attacker_color = 0x15;
  constexpr int defender_color = 0x1B;

  const auto draw_bar = [&](int x, int energy, int color) {
    const int fill_h = std::clamp(energy * bar_max_h / 100, 0, bar_max_h);
    ctx.surface->fill_rect(
        ViewRect{x, bar_bottom - bar_max_h, bar_w, bar_max_h}, empty_color);
    if (fill_h > 0) {
      ctx.surface->fill_rect(ViewRect{x, bar_bottom - fill_h, bar_w, fill_h},
                             color);
    }
  };

  if (attack_attacker_energy_) {
    draw_bar(attacker_x, *attack_attacker_energy_, attacker_color);
  }
  if (attack_defender_energy_) {
    draw_bar(defender_x, *attack_defender_energy_, defender_color);
  }

  if (attack_contamination_) {
    constexpr int contamination_text_y = 162;
    constexpr int contamination_color = 14; // white
    const TextStyle ts{
        .color = contamination_color,
        .align = TextAnchor::Middle,
    };
    context_.font->render_line_onto(
        *ctx.surface,
        compat::format("Contamination: {}%", *attack_contamination_),
        ViewPoint{ctx.surface->width() / 2, contamination_text_y},
        FontFace::SUPERSMA, ts);
  }

  if (police_coming_) {
    constexpr int police_text_color = 18;
    constexpr int police_text_y = 4;
    const TextStyle ts{
        .color = police_text_color,
        .align = TextAnchor::Middle,
    };
    context_.font->render_line_onto(
        *ctx.surface, "POLICE COMING!",
        ViewPoint{ctx.surface->width() / 2, police_text_y}, FontFace::SUPERSMA,
        ts);
  }
}

void StatusDisplay::set_attack_status(std::optional<int> attacker_energy,
                                      std::optional<int> defender_energy) {
  if (attack_mode_ && attack_attacker_energy_ == attacker_energy &&
      attack_defender_energy_ == defender_energy) {
    return;
  }
  attack_mode_ = true;
  attack_attacker_energy_ = attacker_energy;
  attack_defender_energy_ = defender_energy;
  mark_dirty();
}

void StatusDisplay::set_police_coming() {
  police_coming_ = true;
  mark_dirty();
}

void StatusDisplay::set_attack_contamination(
    std::optional<int> contamination_pct) {
  if (attack_contamination_ == contamination_pct) {
    return;
  }
  attack_contamination_ = contamination_pct;
  mark_dirty();
}

void StatusDisplay::clear_attack_status() {
  if (!attack_mode_) {
    return;
  }
  attack_mode_ = false;
  attack_attacker_energy_.reset();
  attack_defender_energy_.reset();
  attack_contamination_.reset();
  police_coming_ = false;
  mark_dirty();
}

void StatusDisplay::clear(const RenderContext& ctx) { ctx.surface->fill(240); }

void StatusDisplay::recipe(StatusDisplayRecipe r) {
  recipe_ = std::move(r);
  mark_dirty();
}
