/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/AlertPopup.h"
#include "views/base/RawRenderer.h"

#include <algorithm>

AlertPopup::AlertPopup(const GameContext& ctx, std::string text, callback_t cb,
                       const RawRenderable* renderable)
    : BasePopup(ctx), text_(std::move(text)), cb_(std::move(cb)),
      renderable_(renderable),
      btn_ok_({button_x_,
               default_popup_height_ - button_height_ - button_bottom_margin_,
               button_width_, button_height_},
              "OK", [this] {
                if (cb_) {
                  cb_();
                }
                finished();
              }) {
  int content_bottom = 0;
  if (renderable_) {
    int picture_width = renderable_->picture_width() * 2; /* scale */
    int picture_height = renderable_->picture_height() * 2;
    auto metrics = font_->size_text(text_, FontFace::ASTANDAR,
                                    text_width_beside_picture(picture_width),
                                    TextStyle{.color = text_color_});
    content_bottom =
        std::max(picture_margin_ + picture_height + text_to_button_gap_,
                 text_top_margin_ + metrics.height + text_to_button_gap_);
  }
  else {
    auto metrics = font_->size_text(text_, FontFace::ASTANDAR, text_area_width_,
                                    TextStyle{.color = text_color_});
    content_bottom = text_top_margin_ + metrics.height + text_to_button_gap_;
  }
  int needed = content_bottom + button_height_ + button_bottom_margin_;
  if (needed > popup_height_) {
    popup_height_ = needed;
  }
  btn_ok_.set_rect({button_x_,
                    popup_height_ - button_height_ - button_bottom_margin_,
                    button_width_, button_height_});

  add_shortcut(input::Action::SubmitInput, &btn_ok_);
  add_shortcut(input::Action::Exit, &btn_ok_);
}

void AlertPopup::add_interactions(views::ui::InteractionContext ictx,
                                  ViewPoint origin) {
  btn_ok_.add_interactions(ictx, origin);
}

void AlertPopup::render(Surface& surface, ViewPoint origin) {
  if (renderable_) {
    RawRenderer renderer;
    auto picture = renderer.surface(*renderable_);

    auto s = popup_surface(popup_width_, popup_height_);
    s.blit_raw(picture, {picture_margin_, picture_margin_});

    int text_x = picture_margin_ + picture.width() + picture_text_gap_;
    font_->render_into_rect(
        s, text_,
        {text_x, text_top_margin_, text_width_beside_picture(picture.width()),
         popup_height_},
        FontFace::ASTANDAR, TextStyle{.color = text_color_});

    btn_ok_.render_with(*this, s);
    surface.blit_raw(s, origin);
  }
  else {
    auto s = popup_surface(popup_width_, popup_height_);
    font_->render_into_rect(s, text_,
                            {text_left_margin_ * 2, text_top_margin_,
                             text_area_width_, popup_height_},
                            FontFace::ASTANDAR,
                            TextStyle{.color = text_color_});
    btn_ok_.render_with(*this, s);
    surface.blit_raw(s, origin);
  }
}
