/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/SaveFileListBox.h"

#include <algorithm>
#include <chrono>
#include "core/time/GameDate.h"
#include "engine/FormatCompat.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/Draw.h"
#include "resources/FontManager.h"

constexpr int SAVE_BOX_W = 224;
constexpr int SAVE_BOX_H = 216;
constexpr int MAX_VISIBLE_FILES = 11;
constexpr int LINE_HEIGHT = 19;
constexpr int NAME_X = 6;
constexpr int DATE_COLUMN_X = 210;
constexpr int NAME_DATE_GAP = 6;
constexpr uint8_t ACTIVE_ROW = 13;
constexpr uint8_t INACTIVE_ROW = 14;
constexpr uint8_t MARKED_ROW = 7;
constexpr uint8_t MARKED_ACTIVE_ROW = 39;
constexpr uint8_t FOCUS_COLOR = 29;
constexpr uint8_t TEXT_COLOR = 1;
constexpr uint8_t DISABLED_TEXT_COLOR = 12;

int SaveFileListBox::max_scroll() const {
  return std::max(0, static_cast<int>(save_files_.size()) - MAX_VISIBLE_FILES);
}

void SaveFileListBox::draw(Surface& screen, ViewPoint p, bool is_selected) {
  auto block = Surface(SAVE_BOX_W, SAVE_BOX_H, PaletteHandle{38}, 15);

  ViewRect r = ViewRect{2, 0, block.width() - 2, block.height()};
  uint8_t white = 15;
  block.fill_rect(r, white);

  int y = 4;
  int visible_count =
      std::min(MAX_VISIBLE_FILES,
               static_cast<int>(save_files_.size()) - *scroll_offset_);

  for (int i = 0; i < visible_count; ++i) {
    int file_index = *scroll_offset_ + i;
    if (file_index >= static_cast<int>(save_files_.size())) {
      break;
    }

    const bool marked =
        is_marked_ && is_marked_(save_files_[file_index].filename);
    if (marked || file_index == cursor_.active) {
      ViewRect highlight_rect{0, y, SAVE_BOX_W, LINE_HEIGHT};
      uint8_t color = is_selected ? ACTIVE_ROW : INACTIVE_ROW;
      if (marked) {
        color = file_index == cursor_.active ? MARKED_ACTIVE_ROW : MARKED_ROW;
      }
      block.fill_rect(highlight_rect, color);
    }

    std::string display_name = save_files_[file_index].filename;
    if (display_name.ends_with(extension_)) {
      display_name =
          display_name.substr(0, display_name.size() - extension_.size());
    }

    auto delta = save_files_[file_index].last_write_time -
                 std::filesystem::file_time_type::clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            std::chrono::system_clock::now() + delta));

    std::tm tm = *std::localtime(&tt);
    std::string display_date =
        compat::format("{} {} {:02d}:{:02d}", tm.tm_mday,
                       short_month_names[tm.tm_mon], tm.tm_hour, tm.tm_min);

    const uint8_t text_color =
        disabled(file_index) ? DISABLED_TEXT_COLOR : TEXT_COLOR;
    const int date_width =
        context_.font->size_text(display_date, FontFace::MINI).width;
    const int name_width = DATE_COLUMN_X - date_width - NAME_X - NAME_DATE_GAP;
    context_.font->render_text_onto(
        block,
        context_.font->truncate_to_width(display_name, FontFace::PIZ_NUMB, name_width),
        ViewPoint{NAME_X, y}, FontFace::PIZ_NUMB, text_color);
    context_.font->render_text_onto(
        block, display_date, ViewPoint{DATE_COLUMN_X, y + 4}, FontFace::MINI,
        text_color, TextAnchor::End);
    y += LINE_HEIGHT;
  }

  scroll_bar_.draw(block, ViewRect{0, 0, SAVE_BOX_W, SAVE_BOX_H},
                   scroll_extent());

  screen.blit_raw(block, p);
  if (is_selected) {
    gfx::draw::box(screen, ViewRect{p.x, p.y, SAVE_BOX_W, SAVE_BOX_H},
                   FOCUS_COLOR, 2);
  }
  height_ = block.height();
}

ScrollBar::Extent SaveFileListBox::scroll_extent() const {
  return {.offset = *scroll_offset_,
          .visible = MAX_VISIBLE_FILES,
          .total = static_cast<int>(save_files_.size())};
}

Size SaveFileListBox::measure() const { return Size{SAVE_BOX_W, SAVE_BOX_H}; }

bool SaveFileListBox::disabled(int index) const {
  return is_disabled_ && is_disabled_(save_files_[index].filename);
}

void SaveFileListBox::handle_action() {
  if (cursor_.active >= 0 &&
      cursor_.active < static_cast<int>(save_files_.size()) &&
      !disabled(cursor_.active)) {
    on_file_clicked_(save_files_[cursor_.active].filename);
  }
}

bool SaveFileListBox::on_adjust(AdjustAxis axis, int delta) {
  if (axis != AdjustAxis::Vertical) {
    return false;
  }
  return cursor_.step(delta, static_cast<int>(save_files_.size()),
                      scroll_offset_, MAX_VISIBLE_FILES);
}

// The file under the given widget-local y, or -1.
int SaveFileListBox::file_at(int local_y) const {
  const int row = (local_y - 4 + 2) / LINE_HEIGHT;
  if (row < 0 || row >= MAX_VISIBLE_FILES) {
    return -1;
  }
  const int index = *scroll_offset_ + row;
  if (index >= static_cast<int>(save_files_.size())) {
    return -1;
  }
  return index;
}

bool SaveFileListBox::on_interaction(views::ui::InteractionEvent ie) {
  if (scroll_bar_.on_interaction(ie, ie.local,
                                 ViewRect{0, 0, SAVE_BOX_W, SAVE_BOX_H},
                                 scroll_extent(), *scroll_offset_)) {
    return true;
  }
  // Consume the press; mouse behavior happens on Click.
  if (ie.type == views::ui::InteractionEvent::Type::ButtonDown) {
    return true;
  }
  if (ie.type == views::ui::InteractionEvent::Type::Hover) {
    const int index = file_at(ie.local.y);
    if (index < 0) {
      return false;
    }
    cursor_.active = index;
    return true;
  }

  if (ie.type == views::ui::InteractionEvent::Type::Scroll) {
    // Scroll up (positive) = decrease offset, scroll down (negative) = increase
    // offset
    const int max_scroll = this->max_scroll();
    if (ie.scroll_delta > 0) {
      // Scroll up
      if (*scroll_offset_ > 0) {
        (*scroll_offset_)--;
        return true;
      }
    }
    else if (ie.scroll_delta < 0) {
      // Scroll down
      if (*scroll_offset_ < max_scroll) {
        (*scroll_offset_)++;
        return true;
      }
    }
    return false;
  }

  if (ie.type == views::ui::InteractionEvent::Type::Click &&
      ie.button == views::ui::MouseButton::Left) {
    const int index = file_at(ie.local.y);
    if (index >= 0 && !disabled(index)) {
      cursor_.active = index;
      on_file_clicked_(save_files_[index].filename);
      return true;
    }
  }

  if (ie.type == views::ui::InteractionEvent::Type::DoubleClick &&
      ie.button == views::ui::MouseButton::Left) {
    const int index = file_at(ie.local.y);
    if (on_file_double_clicked_ && index >= 0 && !disabled(index)) {
      cursor_.active = index;
      on_file_double_clicked_(save_files_[index].filename);
      return true;
    }
  }

  return false;
}
