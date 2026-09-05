/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/SaveFileListBox.h"

#include <chrono>
#include "core/time/GameDate.h"
#include "engine/FormatCompat.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/Draw.h"
#include "resources/FontManager.h"
#include "views/ui/widgets/ScrollThumb.h"

constexpr int SAVE_BOX_W = 224;
constexpr int SAVE_BOX_H = 216;
constexpr int MAX_VISIBLE_FILES = 11;
constexpr int LINE_HEIGHT = 19;
constexpr int DATE_COLUMN_X = 210;
constexpr uint8_t ACTIVE_ROW = 13;
constexpr uint8_t INACTIVE_ROW = 14;
constexpr uint8_t FOCUS_COLOR = 29;

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

    if (file_index == cursor_.active) {
      ViewRect highlight_rect{0, y, SAVE_BOX_W, LINE_HEIGHT};
      block.fill_rect(highlight_rect, is_selected ? ACTIVE_ROW : INACTIVE_ROW);
    }

    // Remove .plsave extension for display
    std::string display_name = save_files_[file_index].filename;
    if (display_name.ends_with(".plsave")) {
      display_name = display_name.substr(0, display_name.size() - 7);
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

    context_.font->render_text_onto(block, display_name, ViewPoint{6, y},
                                    FontFace::PIZ_NUMB, 1);
    context_.font->render_text_onto(block, display_date,
                                    ViewPoint{DATE_COLUMN_X, y + 4},
                                    FontFace::MINI, 1, TextAnchor::End);
    y += LINE_HEIGHT;
  }

  draw_scroll_thumb(block, Size{SAVE_BOX_W, SAVE_BOX_H}, *scroll_offset_,
                    MAX_VISIBLE_FILES, static_cast<int>(save_files_.size()));

  screen.blit_raw(block, p);
  if (is_selected) {
    gfx::draw::box(screen, ViewRect{p.x, p.y, SAVE_BOX_W, SAVE_BOX_H},
                   FOCUS_COLOR, 2);
  }
  height_ = block.height();
}

Size SaveFileListBox::measure() const { return Size{SAVE_BOX_W, SAVE_BOX_H}; }

void SaveFileListBox::handle_action() {
  if (cursor_.active >= 0 &&
      cursor_.active < static_cast<int>(save_files_.size())) {
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
    int max_scroll =
        std::max(0, static_cast<int>(save_files_.size()) - MAX_VISIBLE_FILES);
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
    if (index >= 0) {
      cursor_.active = index;
      on_file_clicked_(save_files_[index].filename);
      return true;
    }
  }

  if (ie.type == views::ui::InteractionEvent::Type::DoubleClick &&
      ie.button == views::ui::MouseButton::Left) {
    const int index = file_at(ie.local.y);
    if (on_file_double_clicked_ && index >= 0) {
      cursor_.active = index;
      on_file_double_clicked_(save_files_[index].filename);
      return true;
    }
  }

  return false;
}
