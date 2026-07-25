/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/ChequePopup.h"
#include "core/util/Tools.h"
#include "engine/input/InputManager.h"
#include "gfx/Blit.h"
#include "views/base/ViewDefs.h"

ChequePopup::ChequePopup(const GameContext& context, callback_t cb)
    : BasePopup(context), context_(context), callback_(std::move(cb)) {
  text_session_ = context_.input->start_text_input(*this);
}

void ChequePopup::render(Surface& surface, ViewPoint origin) {
  Surface pop(views::base::MAIN_PANEL_WIDTH, views::base::MAIN_PANEL_HEIGHT,
              PaletteHandle{0}, 0);
  gfx::blit::blit_raw_with_palette(pop, vga_->get(BACKGROUND_ASSET));

  constexpr uint8_t text_color = 240;

  // Amount field (always visible)
  font_->render_text_onto(pop, "Amount", amount_label_pos_, FontFace::PIZ_NUMB,
                          text_color);

  std::string amount_display = amount_buffer_;
  if (active_field_ == Field::AMOUNT) {
    amount_display += "_";
  }
  font_->render_text_onto(pop, amount_display,
                          ViewPoint{amount_box_.x + 4, amount_box_.y + 2},
                          FontFace::PIZ_NUMB, text_color);

  // Account field (only after amount is confirmed)
  if (active_field_ == Field::ACCOUNT) {
    font_->render_text_onto(pop, "Account no.:", account_label_pos_,
                            FontFace::PIZ_NUMB, text_color);

    std::string account_display = account_buffer_ + "_";
    font_->render_text_onto(pop, account_display,
                            ViewPoint{account_box_.x + 4, account_box_.y + 2},
                            FontFace::PIZ_NUMB, text_color);
  }

  surface.blit_raw(pop, origin);
}

bool ChequePopup::on_text(const input::TextEvent& ev) {
  if (ev.text.length() != 1 || !std::isdigit(ev.text[0])) {
    return false;
  }

  auto& buffer =
      (active_field_ == Field::AMOUNT) ? amount_buffer_ : account_buffer_;
  if (static_cast<int>(buffer.length()) >= MAX_DIGITS) {
    return true;
  }

  buffer.append(ev.text);
  mark_dirty();
  return true;
}

bool ChequePopup::on_action(const input::ActionEvent& ev) {
  if (ev.state.pressed) {
    return false;
  }

  switch (ev.action) {
  case input::Action::SubmitInput:
    if (active_field_ == Field::AMOUNT) {
      confirm_amount();
    }
    else {
      submit();
    }
    return true;

  case input::Action::Backspace: {
    auto& buffer =
        (active_field_ == Field::AMOUNT) ? amount_buffer_ : account_buffer_;
    if (!buffer.empty()) {
      buffer.pop_back();
      mark_dirty();
    }
    return true;
  }

  case input::Action::EraseLine: {
    auto& buffer =
        (active_field_ == Field::AMOUNT) ? amount_buffer_ : account_buffer_;
    if (!buffer.empty()) {
      buffer.clear();
      mark_dirty();
    }
    return true;
  }

  case input::Action::CancelInput:
    finished();
    return true;

  default:
    return false;
  }
}

void ChequePopup::confirm_amount() {
  active_field_ = Field::ACCOUNT;
  mark_dirty();
}

void ChequePopup::submit() {
  int amount = 0;
  if (!amount_buffer_.empty()) {
    amount = std::stoi(amount_buffer_);
  }
  int account = Tools::string_to_int(account_buffer_);
  if (callback_) {
    callback_(amount, account);
  }
  finished();
}
