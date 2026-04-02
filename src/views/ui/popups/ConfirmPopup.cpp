/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/ConfirmPopup.h"
#include "engine/input/InputManager.h"
#include "views/base/RawRenderer.h"

ConfirmPopup::ConfirmPopup(const GameContext& ctx, std::string text,
                           callback_t cb, const RawRenderable* renderable)
    : BasePopup(ctx), text_(std::move(text)), cb_(std::move(cb)),
      renderable_(renderable),
      btn_yes_({30,
                renderable ? (text_left_margin_ +
                              renderable->picture_height() * 2 /* scale */ +
                              text_left_margin_)
                           : 80,
                74, 38},
               "Yes", [this] { confirm(true); }),
      btn_no_({240,
               renderable ? (text_left_margin_ +
                             renderable->picture_height() * 2 /* scale */ +
                             text_left_margin_)
                          : 80,
               74, 38},
              "No", [this] { confirm(false); }) {
  input_ = ctx.input->register_listener(*this);
}

void ConfirmPopup::confirm(bool yes) {
  if (cb_) {
    cb_(yes);
  }
  finished();
}

bool ConfirmPopup::on_action(const input::ActionEvent& ev) {
  if (ev.state.pressed) {
    return false;
  }
  switch (ev.action) {
  case input::Action::ConfirmYes:
    confirm(true);
    return true;
  case input::Action::ConfirmNo:
    confirm(false);
    return true;
  default:
    return false;
  }
}

void ConfirmPopup::add_interactions(views::ui::InteractionContext ictx,
                                    ViewPoint origin) {
  btn_yes_.add_interactions(ictx, origin);
  btn_no_.add_interactions(ictx, origin);
}

void ConfirmPopup::render(Surface& surface, ViewPoint origin) {
  int height = popup_height_;
  int text_x = text_left_margin_;
  constexpr int text_color = 10;

  if (renderable_) {
    // Render character picture on the left
    RawRenderer renderer;
    auto picture = renderer.surface(*renderable_);

    // Calculate popup height based on picture size + margins + button height
    height = text_left_margin_ + picture.height() + text_left_margin_ + 38 +
             button_bottom_margin_;

    auto s = popup_surface(popup_width_, height);
    s.blit_raw(picture, {text_left_margin_, text_left_margin_});

    // Render text on the right (after the picture + some spacing)
    text_x = text_left_margin_ + picture.width() + 10;
    font_->render_text_onto(s, text_, {text_x, text_left_margin_},
                            FontFace::ASTANDAR, text_color);

    btn_yes_.render_with(*this, s);
    btn_no_.render_with(*this, s);
    surface.blit_raw(s, origin);
  }
  else {
    // No picture, use default layout
    auto s = popup_surface(popup_width_, height);
    font_->render_text_onto(s, text_, {text_left_margin_, text_left_margin_},
                            FontFace::ASTANDAR, text_color);

    btn_yes_.render_with(*this, s);
    btn_no_.render_with(*this, s);
    surface.blit_raw(s, origin);
  }
}
