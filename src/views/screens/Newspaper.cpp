/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/Newspaper.h"

#include <cassert>
#include <format>
#include "core/city/City.h"
#include "core/food/Ingredient.h"
#include "core/newspaper/ArticleDatabase.h"
#include "core/newspaper/NewspaperState.h"
#include "core/state/GameState.h"
#include "core/util/GameContext.h"
#include "engine/Random.h"
#include "gfx/Blit.h"
#include "gfx/Draw.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"
#include "views/base/ViewDefs.h"

Newspaper::Newspaper(GameContext& context, GameState& state)
    : MainPanelView(context, state) {

  auto* city = state.active_city();
  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back", [city, &state] {
                             // Mark newspaper as read when closing
                             city->newspaper_state().mark_as_read();
                             state.ui().pop();
                           });

  // Decode article_id encoding to resolve the article:
  //   1-50:  flavor text from ARTIKEL0.E
  //   51-100: event article from ARTIKEL1.E (index = article_id - 50)
  //   0xC8:  ingredient template from ARTIKEL3.E
  auto& ns = city->newspaper_state();
  auto& db = state.article_database();

  // If no article has been generated yet (e.g. first day in a new city),
  // seed a random flavor article so the newspaper isn't blank.
  if (ns.article_id == 0) {
    ns.article_id = engine::random::rand_between(1, 50);
    ns.cooldown = core::newspaper::NewspaperState::FALLBACK_COOLDOWN;
  }

  if (ns.is_ingredient_article()) {
    resolved_article_ = db.format_ingredient_article(
        ns.category, ns.polarity, to_string(*ns.target_ingredient));
    current_article_ = &resolved_article_;
  }
  else if (ns.is_event_article()) {
    // Event articles (51-100) -> ARTIKEL1.E, index = article_id - 50
    current_article_ = db.get_article(1, ns.event_article_index());
  }
  else if (ns.article_id > 0) {
    // Flavor text (1-50) -> ARTIKEL0.E
    current_article_ = db.get_article(0, ns.article_id);
  }
  // Mark the article as seen:
  ns.cooldown = 0;
}

void Newspaper::tick() {}

void Newspaper::render(const RenderContext& ctx) {
  // Draw newspaper background
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back31"));

  constexpr int bgcolor_index = 159;
  auto date_box = ViewRect{252, 34, 100, 22};
  ctx.surface->fill_rect(date_box, bgcolor_index);

  // Render date in newspaper header
  auto date_str = state_.time().current_date().to_string();
  context_.font->render_text_onto(*ctx.surface, date_str,
                                  date_box.point() + PxOffset{0, 4},
                                  FontFace::SUPERSMA, 240);

  constexpr int title_color = 240;

  // Render article if available
  if (current_article_) {
    int picture_offset_y = 0;
    bool has_image = current_article_->image_id > 0;

    // Title always renders full width, but body text is constrained to before
    // the sidebar when there's no image.
    constexpr int full_width = 316;
    constexpr int narrow_width = 216;

    Surface title_surface(Size{full_width, 58}, ctx.surface->palette());
    title_surface.fill(bgcolor_index);

    int body_width = has_image ? full_width : narrow_width;
    Surface text_surface(Size{body_width, 212}, ctx.surface->palette());
    text_surface.fill(bgcolor_index);

    // Font selection: Try RUBY first, fallback to ASTANDAR if text wraps
    FontFace title_font = FontFace::RUBY;

    // Count how many lines the title would wrap to with RUBY font
    int line_count = context_.font->count_wrapped_lines(
        current_article_->title, FontFace::RUBY, title_surface.width());

    // If it wraps to more than 2 lines, use ASTANDAR instead
    if (line_count > 2) {
      title_font = FontFace::ASTANDAR;
    }

    // Render title centered on the surface
    context_.font->render_text_onto(title_surface, current_article_->title,
                                    ViewPoint{title_surface.width() / 2, 0},
                                    title_font, title_color,
                                    AlignX::CenterPoint);
    // Render article image if present
    if (has_image) {
      // Blank out the side column, if we have a picture we don't display
      // that and we center the image instead and center the text underneath it.
      auto side_column_box = ViewRect{250, 60, 100, 284};
      ctx.surface->fill_rect(side_column_box, bgcolor_index);

      auto image_path = compat::format("z/zb{}", current_article_->image_id);
      const auto& image = context_.vga->get(image_path);
      ctx.surface->blit_raw(image, {90, 130});
      picture_offset_y = image.height() + 20;
    }

    context_.font->render_text_onto(text_surface, current_article_->body,
                                    ViewPoint{4, 0}, FontFace::SUPERSMA, 240);

    const ViewPoint title_pos{34, 74};
    ctx.surface->blit_raw(title_surface, title_pos);
    ctx.surface->blit_raw(
        text_surface,
        title_pos + PxOffset{0, title_surface.height() + picture_offset_y});
  }
  else {
    Logger::warn("No current article");
  }
}
