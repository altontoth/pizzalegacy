/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/TextInputPopup.h"
#include "engine/input/InputManager.h"
#include "gfx/Surface.h"
#include "resources/util/TextEncoding.h"

#include <algorithm>

TextInputPopup::TextInputPopup(const GameContext& context, std::string text,
                               int max_characters, callback_t cb)
    : BasePopup(context), prompt_text_(std::move(text)),
      max_characters_(max_characters), callback_(std::move(cb)) {
  text_ = context.input->start_text_input(*this);

  const int lines = std::max(
      1, font_->count_wrapped_lines(prompt_text_, prompt_face_, prompt_width_));
  const int height =
      font_->size_text(prompt_text_, prompt_face_, prompt_width_).height;
  const int line_height = height / lines;
  extra_prompt_height_ = height - line_height;
}

void TextInputPopup::render(Surface& surface, ViewPoint origin) {
  auto pop_surface =
      popup_surface(popup_width_, popup_height_ + extra_prompt_height_);

  const uint8_t text_box_bg = 13;
  const uint8_t text_color = 240;

  font_->render_into_rect(
      pop_surface, prompt_text_,
      ViewRect{pop_surface.rect().center().x, prompt_y_, prompt_width_,
               popup_height_},
      prompt_face_,
      TextStyle{.color = text_color, .align = TextAnchor::Middle});

  const ViewRect bg_box = {30, input_box_top_ + extra_prompt_height_, 302,
                           input_box_height_};
  pop_surface.fill_rect(bg_box, text_box_bg);

  std::string display_text = input_text_;
  display_text.push_back('_');

  constexpr int text_box_padding = 4;
  const ViewPoint input_origin{bg_box.x + text_box_padding,
                               bg_box.y + text_box_padding};
  font_->render_text_onto(pop_surface, display_text, input_origin, prompt_face_,
                          text_color, TextAnchor::Start);

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
  if (!ev.state.pressed) {
    return false;
  }
  switch (ev.action) {
  case input::Action::SubmitInput:
    submit();
    return true;
  case input::Action::CancelInput:
    // Only a submitted answer closes this popup.
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

void TextInputPopup::backspace() {
  if (!input_text_.empty()) {
    text::pop_back_utf8(input_text_);
  }
  mark_dirty();
}

void TextInputPopup::add_interactions(views::ui::InteractionContext /*unused*/,
                                      ViewPoint /*unused*/) {}
