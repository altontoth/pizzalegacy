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
      btn_ok_({30,
               renderable ? (text_left_margin_ +
                             (renderable->picture_height() * 2) /* scale */ +
                             text_left_margin_)
                          : 80,
               74, 38},
              "OK", [this] {
                if (cb_) {
                  cb_();
                }
                finished();
              }) {}

void AlertPopup::add_interactions(views::ui::InteractionContext ictx,
                                  ViewPoint origin) {
  btn_ok_.add_interactions(ictx, origin);
}

void AlertPopup::render(Surface& surface, ViewPoint origin) {
  int height = popup_height_;
  int text_x = text_left_margin_;
  constexpr int text_color = 9;

  if (renderable_) {
    // Render character picture on the left
    RawRenderer renderer;
    auto picture = renderer.surface(*renderable_);

    height = text_left_margin_ + picture.height() + text_left_margin_ + 38 +
             button_bottom_margin_;

    auto s = popup_surface(popup_width_, height);
    const int border_size = 10;
    s.blit_raw(picture, {border_size, border_size});

    // Render text on the right (after the picture + some spacing)
    text_x = text_left_margin_ + picture.width() + 10;
    font_->render_into_rect(s, text_, {text_x, text_top_margin_, 247, 300},
                            FontFace::ASTANDAR, TextStyle{.color = text_color});

    btn_ok_.render_with(*this, s);
    surface.blit_raw(s, origin);
  }
  else {
    // No picture, use default layout
    auto s = popup_surface(popup_width_, height);
    font_->render_into_rect(s, text_,
                            {text_left_margin_ * 2, text_top_margin_, 247, 300},
                            FontFace::ASTANDAR, TextStyle{.color = text_color});

    btn_ok_.render_with(*this, s);
    surface.blit_raw(s, origin);
  }
}
