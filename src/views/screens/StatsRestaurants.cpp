/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/StatsRestaurants.h"

#include "core/city/City.h"
#include "core/city/District.h"
#include "core/city/Property.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/util/Constants.h"
#include "core/util/LegacyConstants.h"
#include "engine/gfx/Draw.h"
#include "resources/FontManager.h"
#include "views/base/ViewDefs.h"
#include "views/ui/PageDecoration.h"

StatsRestaurants::StatsRestaurants(GameContext& context, GameState& state,
                                   core::PlayerId player_id)
    : MainPanelView(context, state), player_id_(player_id),
      stanbobs_(context_.bob->get("bob/stanbobs.bob")),
      tiles_medium_(context_.vga->get("gfx/med1")) {
  create_buttons();
}

void StatsRestaurants::tick() {}

void StatsRestaurants::create_buttons() {
  using BT = ButtonType;

  // Next page button
  input_panel_->add_button(Icon::ARROW_UP, BT::Horizontal,
                           ButtonPosition::TopLeft, "Next Page", [this]() {
                             if (current_page_ < max_pages_) {
                               current_page_++;
                               mark_dirty();
                             }
                           });
  //
  // Previous page button
  input_panel_->add_button(Icon::ARROW_DOWN, BT::Horizontal,
                           ButtonPosition::MiddleLeft, "Previous Page",
                           [this]() {
                             if (current_page_ > 0) {
                               current_page_--;
                               mark_dirty();
                             }
                           });

  // Back button
  input_panel_->add_button(Icon::ARROW_BACK, BT::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this]() { state_.ui().pop(); });
}

void StatsRestaurants::render(const RenderContext& ctx) {
  ctx.surface->palette(PaletteHandle{68});
  ctx.surface->fill({15});
  const int edge_offset = 6;
  views::ui::render_decorated_corners(
      *ctx.surface, stanbobs_,
      ViewRect{{edge_offset, edge_offset},
               {views::base::MAIN_PANEL_WIDTH - (edge_offset * 2),
                views::base::MAIN_PANEL_HEIGHT - (edge_offset * 2)}},
      4, {11});

  TextStyle ts{.color = 240,
               .align = TextAnchor::Start,
               .shadow = ShadowStyle{
                   .offset = {-2, 2},
                   .color = 13,
               }};
  context_.font->render_text_onto(*ctx.surface, "restaurant overview",
                                  ViewPoint{56, 24}, FontFace::ASTANDAR, ts);

  auto restaurants =
      state_.restaurants_in_city(player_id_, state_.active_city_id());
  max_pages_ =
      restaurants.empty() ? 0 : (static_cast<int>(restaurants.size()) - 1) / 2;

  const int base_idx = current_page_ * 2;
  const ViewPoint slot_positions[2] = {{16, 40}, {16, 168}};

  for (int i = 0;
       i < 2 && (base_idx + i) < static_cast<int>(restaurants.size()); ++i) {
    draw_restaurant_overview(ctx, slot_positions[i], restaurants[base_idx + i]);
  }
}

void StatsRestaurants::draw_restaurant_overview(const RenderContext& ctx,
                                                ViewPoint pos, Restaurant* r) {
  const City& city = r->property().city();
  constexpr uint8_t label_color = 13;
  constexpr int title_y = 12;

  // Restaurant name
  context_.font->render_text_onto(*ctx.surface, r->name(),
                                  pos + PxOffset{12, title_y},
                                  FontFace::PIZ_NUMB, label_color);

  // District name
  context_.font->render_text_onto(*ctx.surface, r->property().district().name(),
                                  pos + PxOffset{208, title_y},
                                  FontFace::PIZ_NUMB, label_color);

  // Map thumbnail: NxN tiles with border and gap.
  constexpr int border_w = 2;
  constexpr int gap = 2;
  const int tile_count = 8;
  const int half = tile_count / 2;
  constexpr int tile_w = LegacyConstants::BASE_MAP_TILE_WIDTH;
  constexpr int tile_h = LegacyConstants::BASE_MAP_TILE_HEIGHT;
  const int inset = border_w + gap;
  const int map_outer_w = (tile_count * tile_w) + (inset * 2);
  const int map_outer_h = (tile_count * tile_h) + (inset * 2);

  const ViewPoint map_pos = pos + PxOffset{12, 40};
  ctx.surface->fill_rect(
      ViewRect{map_pos.x, map_pos.y, map_outer_w, map_outer_h}, 11);
  ctx.surface->fill_rect(ViewRect{map_pos.x + border_w, map_pos.y + border_w,
                                  map_outer_w - (border_w * 2),
                                  map_outer_h - (border_w * 2)},
                         15);

  const auto index = r->property().index();
  const int cx = City::index_to_x(index);
  const int cy = City::index_to_y(index);

  for (int dy = -half; dy < half; ++dy) {
    for (int dx = -half; dx < half; ++dx) {
      int map_x = cx + dx;
      int map_y = cy + dy;
      if (map_x < 0 || map_x >= MAP_TILES_PER_ROW || map_y < 0 ||
          map_y >= MAP_TILES_PER_COLUMN) {
        continue;
      }

      uint8_t type = city.type(City::x_y_to_index(map_x, map_y));
      ViewRect src = landsym_source_rect(type);
      ViewPoint dst{map_pos.x + inset + ((dx + half) * tile_w),
                    map_pos.y + inset + ((dy + half) * tile_h)};
      ctx.surface->blit(tiles_medium_, src, dst);
    }
  }

  // Sales/profit table
  const auto& stats = r->stats();
  Money day_sales = stats.yesterday.revenue;
  Money day_profit = stats.yesterday.gross_profit;

  auto week_data = stats.last_week();
  Money week_sales{0};
  Money week_profit{0};
  for (const auto& day : week_data) {
    week_sales = week_sales + day.revenue;
    week_profit = week_profit + day.gross_profit;
  }

  // Table layout positions (relative to pos)
  constexpr int table_x = 104;
  constexpr int day_col_x = 244;
  constexpr int week_col_x = 334;
  constexpr int header_y = 36;
  constexpr int sales_y = 58;
  constexpr int profit_y = 84;
  constexpr int table_col_header_color = 11;
  constexpr int table_row_header_color = 13;
  constexpr int table_value_color = 240;
  constexpr PxOffset vert_bar_1_orig{182, 36};
  constexpr PxOffset vert_bar_2_orig{262, 36};
  constexpr PxOffset horiz_bar_1_orig{122, 54};
  constexpr PxOffset horiz_bar_2_orig{122, 78};
  constexpr int vert_bar_length = 70;
  constexpr int horiz_bar_length = 222;
  constexpr PxOffset vert_bar_1_end{vert_bar_1_orig.dx,
                                    vert_bar_1_orig.dy + vert_bar_length};
  constexpr PxOffset vert_bar_2_end{vert_bar_2_orig.dx,
                                    vert_bar_2_orig.dy + vert_bar_length};
  constexpr PxOffset horiz_bar_1_end{horiz_bar_1_orig.dx + horiz_bar_length,
                                     horiz_bar_1_orig.dy};
  constexpr PxOffset horiz_bar_2_end{horiz_bar_2_orig.dx + horiz_bar_length,
                                     horiz_bar_2_orig.dy};

  // Column headers
  context_.font->render_text_onto(
      *ctx.surface, "day", pos + PxOffset{day_col_x, header_y},
      FontFace::PIZ_NUMB, table_col_header_color, TextAnchor::End);
  context_.font->render_text_onto(
      *ctx.surface, "week", pos + PxOffset{week_col_x, header_y},
      FontFace::PIZ_NUMB, table_col_header_color, TextAnchor::End);

  // Table lines
  gfx::draw::line(*ctx.surface, pos + vert_bar_1_orig, pos + vert_bar_1_end,
                  table_col_header_color, 2);
  gfx::draw::line(*ctx.surface, pos + vert_bar_2_orig, pos + vert_bar_2_end,
                  table_col_header_color, 2);
  gfx::draw::line(*ctx.surface, pos + horiz_bar_1_orig, pos + horiz_bar_1_end,
                  table_col_header_color, 2);
  gfx::draw::line(*ctx.surface, pos + horiz_bar_2_orig, pos + horiz_bar_2_end,
                  table_col_header_color, 2);

  // Sales row
  context_.font->render_text_onto(*ctx.surface, "sales",
                                  pos + PxOffset{table_x, sales_y},
                                  FontFace::PIZ_NUMB, table_row_header_color);
  context_.font->render_text_onto(*ctx.surface, compat::format("{}", day_sales),
                                  pos + PxOffset{day_col_x, sales_y},
                                  FontFace::PIZ_NUMB, table_value_color,
                                  TextAnchor::End);
  context_.font->render_text_onto(
      *ctx.surface, compat::format("{}", week_sales),
      pos + PxOffset{week_col_x, sales_y}, FontFace::PIZ_NUMB,
      table_value_color, TextAnchor::End);

  // Profit row
  context_.font->render_text_onto(*ctx.surface, "profit",
                                  pos + PxOffset{table_x, profit_y},
                                  FontFace::PIZ_NUMB, table_row_header_color);
  context_.font->render_text_onto(
      *ctx.surface, compat::format("{}", day_profit),
      pos + PxOffset{day_col_x, profit_y}, FontFace::PIZ_NUMB,
      table_value_color, TextAnchor::End);
  context_.font->render_text_onto(
      *ctx.surface, compat::format("{}", week_profit),
      pos + PxOffset{week_col_x, profit_y}, FontFace::PIZ_NUMB,
      table_value_color, TextAnchor::End);

  // Restaurant separator line
  gfx::draw::line(*ctx.surface, ViewPoint{pos.x, pos.y + 124},
                  ViewPoint{pos.x + 302, pos.y + 124}, 13, 2);
}
