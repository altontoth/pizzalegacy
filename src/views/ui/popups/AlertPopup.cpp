/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/AlertPopup.h"
#include "views/base/RawRenderer.h"

AlertPopup::AlertPopup(const GameContext& ctx, std::string text, callback_t cb,
                       const RawRenderable* renderable)
    : BasePopup(ctx), text_(std::move(text)), cb_(std::move(cb)),
      renderable_(renderable),
      btn_ok_({button_x_,
               renderable ? (text_left_margin_ +
                             (renderable->picture_height() * 2) /* scale */ +
                             text_left_margin_)
                          : default_popup_height_ - button_height_ -
                                button_bottom_margin_,
               button_width_, button_height_},
              "OK", [this] {
                if (cb_) {
                  cb_();
                }
                finished();
              }) {
  if (!renderable_) {
    auto metrics = font_->size_text(text_, FontFace::ASTANDAR, text_area_width_,
                                    TextStyle{.color = text_color_});
    int needed = text_top_margin_ + metrics.height + text_to_button_gap_ +
                 button_height_ + button_bottom_margin_;
    if (needed > popup_height_) {
      popup_height_ = needed;
      btn_ok_.set_rect({button_x_,
                        popup_height_ - button_height_ - button_bottom_margin_,
                        button_width_, button_height_});
    }
  }
}

void AlertPopup::add_interactions(views::ui::InteractionContext ictx,
                                  ViewPoint origin) {
  btn_ok_.add_interactions(ictx, origin);
}

void AlertPopup::render(Surface& surface, ViewPoint origin) {
  if (renderable_) {
    RawRenderer renderer;
    auto picture = renderer.surface(*renderable_);

    int height = text_left_margin_ + picture.height() + text_left_margin_ +
                 button_height_ + button_bottom_margin_;
    auto s = popup_surface(popup_width_, height);

    const int border_size = 10;
    s.blit_raw(picture, {border_size, border_size});

    int text_x = text_left_margin_ + picture.width() + 10;
    font_->render_into_rect(s, text_, {text_x, text_top_margin_, 246, 300},
                            FontFace::ASTANDAR,
                            TextStyle{.color = text_color_});

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
