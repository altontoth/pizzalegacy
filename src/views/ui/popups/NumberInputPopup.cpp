/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/NumberInputPopup.h"
#include <algorithm>
#include <cmath>
#include "engine/input/InputManager.h"

NumberInputPopup::NumberInputPopup(const GameContext& context,
                                   const std::string& text,
                                   const std::string& append_text, int value,
                                   int min, int max, int step_size,
                                   callback_t cb)
    : BasePopup(context), context_(context), text_(text),
      append_text_(append_text), value_(value), min_(min), max_(max),
      step_size_(step_size), callback_(std::move(cb)),
      btn_up_{up_rect, "",
              [this] {
                value_ = std::min(value_ + step_size_, max_);
                mark_dirty();
              }},
      btn_down_{down_rect, "",
                [this] {
                  value_ = std::max(value_ - step_size_, min_);
                  mark_dirty();
                }},
      btn_ok_{ok_rect, "OK", [this] {
                if (callback_) {
                  callback_(value_);
                }
                finished();
              }} {

  // attach painters (overlay arrow sprites)
  btn_up_.set_painter(
      [this](Surface& target, ViewPoint top_left, bool pressed) {
        auto up_arrow = bob_->get("bob/stanbobs.bob").frame(39);
        // nudge when pressed to match the pressed chrome
        const PxOffset offset{7 + (pressed ? 1 : 0), 6 + (pressed ? 1 : 0)};
        target.blit_masked(up_arrow, top_left + offset);
      });
  btn_down_.set_painter(
      [this](Surface& target, ViewPoint top_left, bool pressed) {
        auto down_arrow = bob_->get("bob/stanbobs.bob").frame(40);
        const PxOffset offset{7 + (pressed ? 1 : 0), 7 + (pressed ? 1 : 0)};
        target.blit_masked(down_arrow, top_left + offset);
      });

  add_shortcut(input::Action::MoveUp, &btn_up_);
  add_shortcut(input::Action::MoveDown, &btn_down_);
  add_shortcut(input::Action::SubmitInput, &btn_ok_);
}

void NumberInputPopup::render(Surface& surface, ViewPoint origin) {
  auto pop = popup_surface(popup_width_, popup_height_);

  // Title
  font_->render_text_onto(pop, text_, {0, 18}, FontFace::ASTANDAR, 6,
                          AlignX::Center);

  const uint8_t text_box_bg = 13;
  pop.fill_rect({67, 60, 102, 24}, text_box_bg);

  // Render number display or editing buffer
  std::string display_text;
  if (is_editing_) {
    // Show the buffer being typed with underscore suffix
    display_text = edit_buffer_ + "_";
  }
  else {
    // Show the current value with append_text
    display_text = std::to_string(value_) + append_text_;
  }

  font_->render_text_onto(pop, display_text, {-70, 65}, FontFace::ASTANDAR, 5,
                          AlignX::Center);

  // Buttons (PopupButton draws the base; we overlay arrows for up/down)
  btn_up_.render_with(*this, pop);
  btn_down_.render_with(*this, pop);
  btn_ok_.render_with(*this, pop);

  surface.blit_raw(pop, origin);
}

void NumberInputPopup::add_interactions(views::ui::InteractionContext ctx,
                                        ViewPoint origin) {
  btn_up_.add_interactions(ctx, origin);
  btn_down_.add_interactions(ctx, origin);
  btn_ok_.add_interactions(ctx, origin);

  // Add interaction for clicking on the number display field
  const auto number_rect = add_origin(number_display_rect, origin);
  ctx.layers.create_frame_item(
      number_rect, "number_input",
      [this, inv = ctx.invalidate](const views::ui::InteractionEvent& ev) {
        if (ev.type == views::ui::InteractionEvent::Type::Click) {
          start_editing();
          inv();
          return;
        }
        if (ev.type == views::ui::InteractionEvent::Type::Scroll) {
          int delta = std::clamp(ev.scroll_delta, -1, 1);
          value_ = std::clamp(value_ + (delta * step_size_), min_, max_);
          mark_dirty();
        }
      });
}

bool NumberInputPopup::on_text(const input::TextEvent& ev) {
  if (!is_editing_) {
    return false;
  }

  if (ev.text.length() != 1 || !std::isdigit(ev.text[0])) {
    return false;
  }

  if (static_cast<int>(edit_buffer_.length()) >= max_digit_count()) {
    return true;
  }

  edit_buffer_.append(ev.text);
  value_ = clamp_input_value(std::stoi(edit_buffer_));
  mark_dirty();
  return true;
}

bool NumberInputPopup::on_action(const input::ActionEvent& ev) {
  if (!is_editing_) {
    return BasePopup::on_action(ev);
  }
  if (ev.state.pressed) {
    return true;
  }

  switch (ev.action) {
  case input::Action::SubmitInput:
    end_editing(true);
    return true;
  case input::Action::CancelInput:
    end_editing(false);
    return true;
  case input::Action::Backspace:
    if (!edit_buffer_.empty()) {
      edit_buffer_.pop_back();
      value_ = edit_buffer_.empty()
                   ? min_
                   : clamp_input_value(std::stoi(edit_buffer_));
      mark_dirty();
    }
    return true;
  case input::Action::EraseLine:
    if (!edit_buffer_.empty()) {
      edit_buffer_.clear();
      value_ = min_;
      mark_dirty();
    }
    return true;
  default:
    return true;
  }
}

void NumberInputPopup::start_editing() {
  if (is_editing_) {
    return;
  }
  is_editing_ = true;
  edit_buffer_.clear();
  text_session_ = context_.input->start_text_input(*this);
  mark_dirty();
}

void NumberInputPopup::end_editing(bool accept_value) {
  is_editing_ = false;
  edit_buffer_.clear();
  text_session_ = input::TextSession(); // Reset to empty session
  mark_dirty();
}

int NumberInputPopup::max_digit_count() const {
  // Allow one extra digit beyond max so users can overshoot and get clamped
  if (max_ < 10) {
    return 2;
  }
  return static_cast<int>(std::floor(std::log10(max_))) + 2;
}

int NumberInputPopup::clamp_input_value(int val) const {
  return std::clamp(val, min_, max_);
}
