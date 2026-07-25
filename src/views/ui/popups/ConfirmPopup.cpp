/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/ConfirmPopup.h"
#include "views/base/RawRenderer.h"

ConfirmPopup::ConfirmPopup(const GameContext& ctx, std::string text,
                           callback_t cb, const RawRenderable* renderable)
    : BasePopup(ctx), text_(std::move(text)), cb_(std::move(cb)),
      renderable_(renderable),
      btn_yes_(
          {30,
           renderable
               ? (((renderable_ ? pic_text_left_margin_ : text_left_margin_) *
                   2) +
                  (renderable->picture_height() * 2))
               : 80,
           74, 38},
          "Yes", [this] { confirm(true); }),
      btn_no_(
          {240,
           renderable
               ? (((renderable_ ? pic_text_left_margin_ : text_left_margin_) *
                   2) +
                  (renderable->picture_height() * 2))
               : 80,
           74, 38},
          "No", [this] { confirm(false); }) {
  add_shortcut(input::Action::ConfirmYes, &btn_yes_);
  add_shortcut(input::Action::SubmitInput, &btn_yes_);
  add_shortcut(input::Action::ConfirmNo, &btn_no_);
  add_shortcut(input::Action::Exit, &btn_no_);
}

void ConfirmPopup::confirm(bool yes) {
  if (cb_) {
    cb_(yes);
  }
  finished();
}

void ConfirmPopup::add_interactions(views::ui::InteractionContext ictx,
                                    ViewPoint origin) {
  btn_yes_.add_interactions(ictx, origin);
  btn_no_.add_interactions(ictx, origin);
}

void ConfirmPopup::render(Surface& surface, ViewPoint origin) {
  int height = popup_height_;
  constexpr int text_color = 10;
  const int border_size = 10;

  if (renderable_) {
    // Render character picture on the left
    RawRenderer renderer;
    auto picture = renderer.surface(*renderable_);

    // Calculate popup height based on picture size + margins + button height
    height = pic_text_top_margin_ + picture.height() + pic_text_top_margin_ +
             38 + button_bottom_margin_;

    auto s = popup_surface(popup_width_, height);
    s.blit_raw(picture, {border_size, border_size});

    // Render text on the right (after the picture + some spacing)
    const int text_x = border_size + picture.width() + pic_text_left_margin_;
    font_->render_into_rect(
        s, text_, {text_x, pic_text_top_margin_ + border_size, 212, 300},
        FontFace::ASTANDAR, TextStyle{.color = text_color});

    btn_yes_.render_with(*this, s);
    btn_no_.render_with(*this, s);
    surface.blit_raw(s, origin);
  }
  else {
    // No picture, use default layout
    auto s = popup_surface(popup_width_, height);
    font_->render_into_rect(s, text_,
                            {border_size + text_left_margin_,
                             border_size + text_top_margin_, 352, 300},
                            FontFace::ASTANDAR, TextStyle{.color = text_color});

    btn_yes_.render_with(*this, s);
    btn_no_.render_with(*this, s);
    surface.blit_raw(s, origin);
  }
}
