/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/SaveFileListBox.h"
#include "engine/geom/Spaces.h"
#include "resources/FontManager.h"

constexpr int SAVE_BOX_W = 224;
constexpr int SAVE_BOX_H = 216;
constexpr int MAX_VISIBLE_FILES = 12;
constexpr int LINE_HEIGHT = 18;

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

    // Draw gray background for hovered item
    if (file_index == hovered_index_) {
      uint8_t gray = 14;
      ViewRect highlight_rect{0, y, SAVE_BOX_W, LINE_HEIGHT};
      block.fill_rect(highlight_rect, gray);
    }

    // Remove .json extension for display
    std::string display_name = save_files_[file_index];
    if (display_name.size() > 5 &&
        display_name.substr(display_name.size() - 5) == ".json") {
      display_name = display_name.substr(0, display_name.size() - 5);
    }

    context_.font->render_text_onto(block, display_name, ViewPoint{6, y},
                                    FontFace::PIZ_NUMB, 1);
    y += LINE_HEIGHT;
  }

  screen.blit_raw(block, p);
  height_ = block.height();
}

Size SaveFileListBox::measure() const { return Size{SAVE_BOX_W, SAVE_BOX_H}; }

void SaveFileListBox::handle_action() {}

bool SaveFileListBox::on_interaction(views::ui::InteractionEvent ie) {
  if (ie.type == views::ui::InteractionEvent::Type::Hover) {
    // Calculate which file index is being hovered
    int relative_y = ie.local.y;
    int line_index = (relative_y - 4 + 2) / LINE_HEIGHT;

    if (line_index >= 0 && line_index < MAX_VISIBLE_FILES) {
      int file_index = *scroll_offset_ + line_index;
      if (file_index < static_cast<int>(save_files_.size())) {
        hovered_index_ = file_index;
        return true;
      }
    }
    hovered_index_ = -1;
    return false;
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
    if (hovered_index_ >= 0 &&
        hovered_index_ < static_cast<int>(save_files_.size())) {
      on_file_clicked_(save_files_[hovered_index_]);
      return true;
    }
  }

  if (ie.type == views::ui::InteractionEvent::Type::DoubleClick &&
      ie.button == views::ui::MouseButton::Left) {
    if (on_file_double_clicked_ && hovered_index_ >= 0 &&
        hovered_index_ < static_cast<int>(save_files_.size())) {
      on_file_double_clicked_(save_files_[hovered_index_]);
      return true;
    }
  }

  return false;
}
