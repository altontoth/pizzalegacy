/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/StatusDisplay.h"

#include "core/character/Player.h"
#include "core/state/GameState.h"
#include "core/util/ConfigManager.h"
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

  interactions_.create_persistent_item(
      ViewRect{views::base::STATUS_PANEL_OFFSET_X,
               views::base::STATUS_PANEL_OFFSET_Y,
               views::base::STATUS_PANEL_WIDTH,
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
      });
}

void StatusDisplay::tick() {
  const auto blocks = state_.time().time_blocks();
  if (previous_time_ != blocks) {
    previous_time_ = blocks;
    mark_dirty();
  }

  if (!frozen_ && state_.active_player()) {
    const auto balance = state_.active_player()->cash_balance();
    if (previous_balance_ != balance) {
      previous_balance_ = balance;
      mark_dirty();
    }
  }
}

void StatusDisplay::render(const RenderContext& ctx) {
  clear(ctx);

  // Render fixed elements (date, time, money)
  render_header_info(ctx);

  // Render the dynamic recipe content
  render_recipe(recipe_, ctx);
}

void StatusDisplay::render_recipe(const StatusDisplayRecipe& recipe,
                                  const RenderContext& ctx) {
  auto to_alignment = [](StatusAlignment a) {
    switch (a) {
    case StatusAlignment::Left:
      return AlignX::Left;
    case StatusAlignment::Center:
      return AlignX::Center;
    case StatusAlignment::Right:
      return AlignX::Right;
      break;
    }
  };

  constexpr int content_top_y = 16;
  constexpr int content_bottom_y = 163;
  constexpr int default_line_h = 12;
  const int color_white = 14;

  constexpr int panel_width = views::base::STATUS_PANEL_WIDTH;

  // Measure item height accounting for word wrap
  auto measure_item = [&](const StatusItem& item) -> int {
    switch (item.kind) {
    case StatusItemKind::Header: {
      int lines = context_.font->count_wrapped_lines(
          item.col1.text, FontFace::SUPERSMA, panel_width);
      return std::max(1, lines) * default_line_h;
    }
    case StatusItemKind::TwoColumn:
    case StatusItemKind::Single:
      return default_line_h;
    case StatusItemKind::Spacer:
      return item.lines * default_line_h;
    }
    return default_line_h;
  };

  // When content overflows, find the first item (from the top) where
  // everything from that item onward fits. Render from there at content_top_y.
  const auto& items = recipe.items();
  size_t start_index = 0;
  if (ConfigManager::instance()->options().qol_improvements) {
    int available = content_bottom_y - content_top_y + 1;
    // Accumulate heights from the end backward to find the cutoff
    int accum = 0;
    for (size_t i = items.size(); i-- > 0;) {
      int h = measure_item(items[i]);
      if (accum + h > available) {
        start_index = i + 1;
        break;
      }
      accum += h;
    }
  }

  int y = content_top_y;
  for (size_t idx = start_index; idx < items.size(); ++idx) {
    const auto& item = items[idx];
    int item_h = measure_item(item);

    // Stop if we're past the bottom
    if (y > content_bottom_y) {
      break;
    }

    int c = (item.color != 0) ? item.color : color_white;
    switch (item.kind) {
    case StatusItemKind::Header: {
      auto metrics = context_.font->render_text_onto(
          *ctx.surface, item.col1.text, ViewPoint{item.col1.x, y},
          FontFace::SUPERSMA, c);
      item_h = metrics.height > 0 ? metrics.height : default_line_h;
      break;
    }
    case StatusItemKind::TwoColumn: {
      const auto& col1 = item.col1;
      const auto& col2 = item.col2;
      context_.font->render_text_onto(*ctx.surface, col1.text,
                                      ViewPoint{col1.x, y}, FontFace::SUPERSMA,
                                      c, to_alignment(col1.align));
      context_.font->render_text_onto(*ctx.surface, col2.text,
                                      ViewPoint{col2.x, y}, FontFace::SUPERSMA,
                                      c, to_alignment(col2.align));
      break;
    }
    case StatusItemKind::Single:
      context_.font->render_line_onto(
          *ctx.surface, item.col1.text, ViewPoint{item.col1.x, y},
          FontFace::SUPERSMA, c, to_alignment(item.col1.align));
      break;
    case StatusItemKind::Spacer:
      break;
    }
    y += item_h;
  }
}

void StatusDisplay::render_header_info(const RenderContext& ctx) {
  constexpr int date_x_offset = 15;
  constexpr int time_x_offset = 110;
  constexpr int date_time_y_offset = 2;
  constexpr int text_color = 16; // yellow

  // Use frozen values if frozen, otherwise read from GameState
  std::string date_str, time_str, money_str;
  if (frozen_) {
    date_str = frozen_date_;
    time_str = frozen_time_;
    money_str = frozen_money_;
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
        .align = AlignX::Left,
        .shadow = std::nullopt,
        .character_spacing = 2,
    };
    context_.font->render_line_onto(*ctx.surface, money_str,
                                    ViewPoint{money_x_offset, money_y_offset},
                                    FontFace::SUPERSMA, ts);
  }
}

void StatusDisplay::clear(const RenderContext& ctx) { ctx.surface->fill(240); }

void StatusDisplay::recipe(StatusDisplayRecipe r) {
  recipe_ = std::move(r);
  mark_dirty();
}

void StatusDisplay::freeze() {
  frozen_ = true;
  // Capture current header state
  const GameTime& time = state_.time();
  frozen_date_ = time.to_date_string();
  frozen_time_ = time.to_time_string();
  if (state_.active_player()) {
    frozen_money_ =
        compat::format("{}", state_.active_player()->cash_balance());
  }
  else {
    frozen_money_ = "";
  }
  mark_dirty();
}

void StatusDisplay::unfreeze() {
  frozen_ = false;
  mark_dirty();
}
