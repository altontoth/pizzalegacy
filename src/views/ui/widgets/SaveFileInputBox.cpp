/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/SaveFileInputBox.h"
#include <cctype>
#include "engine/geom/Spaces.h"
#include "engine/gfx/Draw.h"
#include "resources/FontManager.h"
#include "resources/util/TextEncoding.h"

constexpr int SAVE_BOX_W = 224;
constexpr int SAVE_BOX_H = 216;
constexpr int MAX_VISIBLE_FILES = 11;
constexpr int LINE_HEIGHT = 19;
constexpr int MAX_FILENAME_LENGTH = 16;
constexpr uint8_t ACTIVE_ROW = 13;
constexpr uint8_t INACTIVE_ROW = 14;
constexpr uint8_t FOCUS_COLOR = 29;

void SaveFileInputBox::draw(Surface& screen, ViewPoint p, bool is_selected) {
  auto block = Surface(SAVE_BOX_W, SAVE_BOX_H, PaletteHandle{38}, 15);

  ViewRect r = ViewRect{2, 0, block.width() - 2, block.height()};
  uint8_t white = 15;
  block.fill_rect(r, white);

  int y = 4;
  int visible_count =
      std::min(MAX_VISIBLE_FILES,
               static_cast<int>(save_files_.size()) + 1 - *scroll_offset_);

  for (int i = 0; i < visible_count; ++i) {
    int list_index = *scroll_offset_ + i;

    // Index 0 is the " - NEW NAME -" entry
    if (list_index == 0) {
      // Draw yellow background if input is active
      if (input_active_) {
        uint8_t yellow = 7;
        // Make the input box the same width as the white area (full width)
        // Start 4 pixels higher (y - 4) and make it 4 pixels taller
        ViewRect input_rect{0, y - 4, SAVE_BOX_W, LINE_HEIGHT + 4};
        block.fill_rect(input_rect, yellow);

        // Add bevel around the yellow input box
        constexpr uint8_t color_bright_yellow = 39;
        constexpr uint8_t color_dark_yellow = 53;
        gfx::draw::beveled_box(block, input_rect, color_bright_yellow,
                               color_dark_yellow, 2);

        // Display the filename being typed with cursor
        std::string display_text = new_filename_ + "_";
        context_.font->render_text_onto(block, display_text, ViewPoint{6, y},
                                        FontFace::PIZ_NUMB, 1);
      }
      else {
        if (list_index == cursor_.active) {
          ViewRect highlight_rect{0, y - 2, SAVE_BOX_W, LINE_HEIGHT};
          block.fill_rect(highlight_rect,
                          is_selected ? ACTIVE_ROW : INACTIVE_ROW);
        }

        context_.font->render_text_onto(block, " - NEW NAME -", ViewPoint{6, y},
                                        FontFace::PIZ_NUMB, 1);
      }
    }
    else {
      // Existing save file
      int file_index =
          list_index - 1; // Offset by 1 because index 0 is NEW NAME

      if (file_index >= static_cast<int>(save_files_.size())) {
        break;
      }

      if (list_index == cursor_.active && !input_active_) {
        ViewRect highlight_rect{0, y, SAVE_BOX_W, LINE_HEIGHT};
        block.fill_rect(highlight_rect,
                        is_selected ? ACTIVE_ROW : INACTIVE_ROW);
      }

      // Remove the file extension for display
      std::string display_name = save_files_[file_index].filename;
      if (display_name.ends_with(extension_)) {
        display_name =
            display_name.substr(0, display_name.size() - extension_.size());
      }

      context_.font->render_text_onto(block, display_name, ViewPoint{6, y},
                                      FontFace::PIZ_NUMB, 1);
    }

    y += LINE_HEIGHT;
  }

  screen.blit_raw(block, p);
  if (is_selected) {
    gfx::draw::box(screen, ViewRect{p.x, p.y, SAVE_BOX_W, SAVE_BOX_H},
                   FOCUS_COLOR, 2);
  }
  height_ = block.height();
}

Size SaveFileInputBox::measure() const { return Size{SAVE_BOX_W, SAVE_BOX_H}; }

void SaveFileInputBox::handle_action() {
  // Row 0 is the NEW NAME entry, files follow shifted by one.
  if (cursor_.active == 0) {
    start_new_name_input();
    return;
  }
  const int file_index = cursor_.active - 1;
  if (file_index >= 0 && file_index < static_cast<int>(save_files_.size())) {
    on_save_callback_(save_files_[file_index].filename);
  }
}

bool SaveFileInputBox::on_adjust(AdjustAxis axis, int delta) {
  if (axis != AdjustAxis::Vertical) {
    return false;
  }
  return cursor_.step(delta, static_cast<int>(save_files_.size()) + 1,
                      scroll_offset_, MAX_VISIBLE_FILES);
}

// The list entry under the given widget-local y (0 is the NEW NAME row,
// files follow shifted by one), or -1.
int SaveFileInputBox::entry_at(int local_y) const {
  const int row = (local_y - 4 + 2) / LINE_HEIGHT;
  if (row < 0 || row >= MAX_VISIBLE_FILES) {
    return -1;
  }
  const int index = *scroll_offset_ + row;
  if (index > static_cast<int>(save_files_.size())) {
    return -1;
  }
  return index;
}

void SaveFileInputBox::start_new_name_input() {
  keyboard_enabled_ = true;
  input_active_ = true;
  new_filename_.clear();
  mark_dirty();
}

bool SaveFileInputBox::on_interaction(views::ui::InteractionEvent ie) {
  // Consume the press; mouse behavior happens on Click.
  if (ie.type == views::ui::InteractionEvent::Type::ButtonDown) {
    return true;
  }
  if (ie.type == views::ui::InteractionEvent::Type::Hover) {
    const int index = entry_at(ie.local.y);
    if (index < 0) {
      return false;
    }
    cursor_.active = index;
    return true;
  }

  if (ie.type == views::ui::InteractionEvent::Type::Scroll) {
    // Scroll up (positive) = decrease offset, scroll down (negative) = increase
    // offset
    int max_scroll = std::max(0, static_cast<int>(save_files_.size()) + 1 -
                                     MAX_VISIBLE_FILES);
    if (ie.scroll_delta > 0) {
      // Scroll up
      if (*scroll_offset_ > 0) {
        (*scroll_offset_)--;
        mark_dirty();
        return true;
      }
    }
    else if (ie.scroll_delta < 0) {
      // Scroll down
      if (*scroll_offset_ < max_scroll) {
        (*scroll_offset_)++;
        mark_dirty();
        return true;
      }
    }
    return false;
  }

  if (ie.type == views::ui::InteractionEvent::Type::Click &&
      ie.button == views::ui::MouseButton::Left) {
    const int index = entry_at(ie.local.y);
    if (index >= 0) {
      cursor_.active = index;
      if (index == 0) {
        start_new_name_input();
        return true;
      }
      const int file_index = index - 1;
      if (file_index < static_cast<int>(save_files_.size())) {
        on_save_callback_(save_files_[file_index].filename);
        return true;
      }
    }
  }

  return false;
}

bool SaveFileInputBox::on_text(const input::TextEvent& ev) {
  if (!input_active_) {
    return false;
  }

  // Filter input: only allow letters and numbers
  std::string filtered_text;
  for (char c : ev.text) {
    if (std::isalnum(static_cast<unsigned char>(c))) {
      filtered_text.push_back(c);
    }
  }

  if (!filtered_text.empty()) {
    new_filename_.append(filtered_text);
    if (new_filename_.size() > MAX_FILENAME_LENGTH) {
      new_filename_.resize(MAX_FILENAME_LENGTH);
    }
    mark_dirty();
  }

  return true;
}

bool SaveFileInputBox::on_action(const input::ActionEvent& ev) {
  if (!input_active_ || !ev.state.pressed) {
    return false;
  }

  switch (ev.action) {
  case input::Action::SubmitInput:
    if (!new_filename_.empty()) {
      // Add the file extension if not present
      std::string filename = new_filename_;
      if (!filename.ends_with(extension_)) {
        filename += extension_;
      }
      on_save_callback_(filename);
    }
    end_input();
    break;
  case input::Action::CancelInput:
    end_input();
    break;
  case input::Action::Backspace:
    if (new_filename_.length() > 0) {
      text::pop_back_utf8(new_filename_);
    }
    break;
  case input::Action::EraseLine:
    new_filename_.clear();
    break;
  default:
    return false;
  }
  mark_dirty();
  return true;
}

void SaveFileInputBox::mark_dirty() {
  if (invalidate_) {
    invalidate_();
  }
}

void SaveFileInputBox::begin_keyboard_session(input::InputManager* input,
                                              std::function<void()> inv) {
  if (!keyboard_enabled_) {
    return;
  }
  invalidate_ = std::move(inv);
  text_session_.emplace(input->start_text_input(*this));
}

void SaveFileInputBox::end_input() {
  input_active_ = false;
  keyboard_enabled_ = false;
  new_filename_.clear();
  text_session_.reset();
}
