/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/TextInputPopup.h"
#include "engine/input/InputManager.h"
#include "gfx/Surface.h"
#include "resources/util/TextEncoding.h"

TextInputPopup::TextInputPopup(const GameContext& context, std::string text,
                               int max_characters, callback_t cb)
    : BasePopup(context), prompt_text_(std::move(text)),
      max_characters_(max_characters), callback_(std::move(cb)) {
  input_ = context.input->register_listener(*this);
  text_ = context.input->start_text_input(*this);
}

void TextInputPopup::render(Surface& surface, ViewPoint origin) {
  auto pop_surface = popup_surface(popup_width_, popup_height_);

  const ViewPoint text_origin{0, 20};
  const uint8_t text_box_bg = 13;
  const uint8_t text_color = 240;

  font_->render_text_onto(pop_surface, prompt_text_, text_origin,
                          FontFace::ASTANDAR, text_color, AlignX::Center);

  const ViewRect bg_box = {30, 60, 302, 24};
  pop_surface.fill_rect(bg_box, text_box_bg);

  std::string display_text = input_text_;
  display_text.push_back('_');

  const ViewPoint input_origin = text_origin + PxOffset{34, 44};
  font_->render_text_onto(pop_surface, display_text, input_origin,
                          FontFace::ASTANDAR, text_color, AlignX::Left);

  surface.blit_raw(pop_surface, origin);
}

bool TextInputPopup::on_text(const input::TextEvent& ev) {
  input_text_ += ev.text;
  if (max_characters_ > 0) {
    text::truncate_utf8(input_text_, static_cast<std::size_t>(max_characters_));
  }
  mark_dirty();
  return true;
}

bool TextInputPopup::on_action(const input::ActionEvent& ev) {
  if (ev.state.pressed) {
    return false;
  }
  switch (ev.action) {
  case input::Action::SubmitInput:
    submit();
    return true;
  case input::Action::CancelInput:
    cancel();
    return true;
  case input::Action::Backspace:
    backspace();
    return true;
  case input::Action::EraseLine:
    if (!input_text_.empty()) {
      input_text_.clear();
      mark_dirty();
    }
    return true;
  default:
    return false;
  }
}
void TextInputPopup::submit() {
  if (callback_) {
    callback_(input_text_);
  }
  finished();
  mark_dirty();
}

void TextInputPopup::cancel() {
  finished();
  mark_dirty();
}

void TextInputPopup::backspace() {
  if (!input_text_.empty()) {
    text::pop_back_utf8(input_text_);
  }
  mark_dirty();
}

void TextInputPopup::add_interactions(views::ui::InteractionContext /*unused*/,
                                      ViewPoint /*unused*/) {}
