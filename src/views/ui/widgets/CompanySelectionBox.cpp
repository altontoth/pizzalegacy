/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/CompanySelectionBox.h"
#include <algorithm>
#include <utility>
#include "engine/geom/Spaces.h"
#include "engine/gfx/Draw.h"
#include "engine/gfx/Surface.h"
#include "engine/input/TextSession.h"
#include "resources/FontManager.h"
#include "resources/util/TextEncoding.h"

constexpr int COMPANY_BOX_W = 226;
constexpr int COMPANY_BOX_H = 224;
constexpr int company_name_box_width = 220;
constexpr int company_name_box_height = 26;
constexpr int company_name_box_y_offset = 2;
constexpr int max_name_length = 14;

void CompanySelectionBox::draw(Surface& screen, ViewPoint p, bool is_selected) {
  auto block = Surface(COMPANY_BOX_W, COMPANY_BOX_H, PaletteHandle{38});

  constexpr uint8_t color_outer_border = 10;
  block.fill(color_outer_border);

  int col_x = 2;
  int this_y = 0;

  constexpr uint8_t color_yellow = 7;
  constexpr uint8_t color_green = 23;
  constexpr uint8_t color_gray = 13;

  constexpr uint8_t color_bright_green = 27;
  constexpr uint8_t color_dark_green = 21;
  constexpr uint8_t color_bright_yellow = 39;
  constexpr uint8_t color_dark_yellow = 53;

  int i = 0;
  constexpr PxOffset text_padding{4, 6};
  for (const auto& player : players_) {
    ViewRect bg(col_x, this_y, company_name_box_width, company_name_box_height);
    block.fill_rect(bg, player.is_selected ? color_yellow
                        : player.is_human  ? color_green
                                           : color_gray);

    if (player.is_selected || player.is_human) {
      int bx = bg.x;
      int by = bg.y;
      int bw = bg.w;
      int bh = bg.h;

      uint8_t bright =
          player.is_selected ? color_bright_yellow : color_bright_green;
      uint8_t dark = player.is_selected ? color_dark_yellow : color_dark_green;

      using gfx::draw::line;

      // top border (bright)
      line(block, {bx, by}, {bx + bw - 1, by}, bright);
      line(block, {bx, by + 1}, {bx + bw - 1, by + 1}, bright);

      // left border (bright)
      line(block, {bx, by}, {bx, by + bh - 1}, bright);

      // right border (dark)
      line(block, {bx + bw - 1, by}, {bx + bw - 1, by + bh - 1}, dark);

      // bottom border (dark)
      line(block, {bx, by + bh - 1}, {bx + bw - 1, by + bh - 1}, dark);

      // highlight squares in "white" (index lookup)
      constexpr uint8_t white_idx = 253;

      block.fill_rect({bx + bw - 2, by, 2, 2}, white_idx);
      block.fill_rect({bx + bw - 4, by + 2, 2, 2}, white_idx);
    }

    bool this_player_renamed =
        (company_being_renamed_ && i == *company_being_renamed_);
    std::string render_name(this_player_renamed ? new_name_ : player.name);
    if (this_player_renamed) {
      render_name.push_back('_');
    }
    context_.font->render_text_onto(block, render_name,
                                    ViewPoint{col_x, this_y} + text_padding,
                                    FontFace::PIZ_NUMB, 1);

    i++;

    this_y += company_name_box_height + company_name_box_y_offset;
  }

  screen.blit_raw(block, p);
  if (is_selected) {
    constexpr uint8_t focus_color = 29;
    gfx::draw::box(screen, ViewRect{p.x, p.y, COMPANY_BOX_W, COMPANY_BOX_H},
                   focus_color, 2);
  }
  height_ = block.height();
  width_ = block.width();
}

void CompanySelectionBox::handle_action() { rename_company(selected_company_); }

bool CompanySelectionBox::on_adjust(AdjustAxis axis, int delta) {
  if (axis != AdjustAxis::Vertical || !on_scroll_) {
    return false;
  }
  on_scroll_(-delta);
  return true;
}

void CompanySelectionBox::begin_keyboard_session(input::InputManager* input,
                                                 std::function<void()> inv) {
  if (!keyboard_enabled_) {
    Logger::warn("Beginning keyboard session when we already have one!");
    return;
  }
  invalidate_ = std::move(inv);
  text_session_.emplace(input->start_text_input(*this));
}

bool CompanySelectionBox::on_text(const input::TextEvent& ev) {
  new_name_.append(ev.text);
  text::truncate_utf8(new_name_, max_name_length);
  mark_dirty();
  return true;
}

void CompanySelectionBox::mark_dirty() {
  if (invalidate_) {
    invalidate_();
  }
}

bool CompanySelectionBox::on_action(const input::ActionEvent& ev) {
  if (!company_being_renamed_ || !ev.state.pressed) {
    return false;
  }
  switch (ev.action) {
  case input::Action::SubmitInput:
    if (!new_name_.empty()) {
      players_[*company_being_renamed_].name = new_name_;
    }
    else {
      new_name_.clear();
    }
    end_rename();
    break;
  case input::Action::CancelInput:
    end_rename();
    break;
  case input::Action::Backspace:
    if (new_name_.length() > 0) {
      text::pop_back_utf8(new_name_);
    }
    break;
  case input::Action::EraseLine:
    new_name_.clear();
    break;
  default:
    return false;
  }
  mark_dirty();
  return true;
}

void CompanySelectionBox::end_rename() {
  company_being_renamed_.reset();
  keyboard_enabled_ = false;
  new_name_.clear();
  text_session_.reset();
}

void CompanySelectionBox::rename_company(int company_id) {
  keyboard_enabled_ = true;
  if (company_id >= 0) {
    company_being_renamed_.emplace(company_id);
  }
}

void CompanySelectionBox::on_company_clicked(
    std::function<void(int playerIndex)> callback) {
  on_company_clicked_ = std::move(callback);
}

Size CompanySelectionBox::measure() const {
  return Size{COMPANY_BOX_W, COMPANY_BOX_H};
}

bool CompanySelectionBox::on_interaction(views::ui::InteractionEvent ie) {
  if (ie.type == views::ui::InteractionEvent::Type::Scroll) {
    // Call scroll callback if set
    if (on_scroll_) {
      on_scroll_(ie.scroll_delta);
      return true;
    }
    return false;
  }

  // Consume the press; mouse behavior happens on Click.
  if (ie.type == views::ui::InteractionEvent::Type::ButtonDown) {
    return true;
  }
  if (ie.type != views::ui::InteractionEvent::Type::Click) {
    return false;
  }
  int clicked_index = std::clamp(
      ie.pos.y / (company_name_box_height + company_name_box_y_offset), 0,
      static_cast<int>(players_.size() - 1));
  if (clicked_index != selected_company_) {
    end_rename();
    players_[selected_company_].is_selected = false;
    selected_company_ = clicked_index;
    players_[selected_company_].is_selected = true;
    on_company_clicked_(selected_company_);
  }
  else {
    rename_company(clicked_index);
  }
  return true;
}
