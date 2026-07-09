/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/TextListBox.h"

#include <algorithm>
#include <utility>
#include "engine/geom/Spaces.h"
#include "resources/FontManager.h"

constexpr int LINE_HEIGHT = 18;
constexpr int TEXT_COLOR = 1;

void TextListBox::set_lines(std::vector<std::string> lines, bool clickable) {
  lines_ = std::move(lines);
  clickable_ = clickable;
  hovered_index_ = -1;
  *scroll_offset_ = std::clamp(*scroll_offset_, 0, max_scroll());
}

int TextListBox::visible_lines() const { return size_.h / LINE_HEIGHT; }

int TextListBox::max_scroll() const {
  return std::max(0, static_cast<int>(lines_.size()) - visible_lines());
}

void TextListBox::draw_scroll_thumb(Surface& block, int total_lines) const {
  const int visible = visible_lines();
  if (visible >= total_lines || total_lines == 0) {
    return;
  }

  constexpr uint8_t thumb_color = 14;
  constexpr int thumb_w = 4;
  constexpr int thumb_margin = 1;

  const int track_h = size_.h;
  const int thumb_h = std::max(10, visible * track_h / total_lines);
  const int scroll = std::min(*scroll_offset_, max_scroll());
  const int thumb_y =
      max_scroll() == 0 ? 0 : scroll * (track_h - thumb_h) / max_scroll();

  ViewRect thumb{size_.w - thumb_w - thumb_margin, thumb_y, thumb_w, thumb_h};
  block.fill_rect(thumb, thumb_color);
}

void TextListBox::draw(Surface& screen, ViewPoint p, bool /*is_selected*/) {
  auto block = Surface(size_.w, size_.h, PaletteHandle{38}, 15);

  ViewRect r = ViewRect{2, 0, block.width() - 2, block.height()};
  constexpr uint8_t white = 15;
  block.fill_rect(r, white);

  int y = 4;
  const int count = static_cast<int>(lines_.size());
  const int visible = std::min(visible_lines(), count - *scroll_offset_);

  for (int i = 0; i < visible; ++i) {
    const int line_index = *scroll_offset_ + i;
    if (line_index >= count) {
      break;
    }

    if (clickable_ && line_index == hovered_index_) {
      constexpr uint8_t gray = 14;
      ViewRect highlight_rect{0, y, size_.w, LINE_HEIGHT};
      block.fill_rect(highlight_rect, gray);
    }

    context_.font->render_text_onto(block, lines_[line_index], ViewPoint{6, y},
                                    FontFace::PIZ_NUMB, TEXT_COLOR);
    y += LINE_HEIGHT;
  }

  if (context_.config->options().qol_improvements) {
    draw_scroll_thumb(block, count);
  }

  screen.blit_raw(block, p);
  height_ = block.height();
}

bool TextListBox::on_interaction(views::ui::InteractionEvent ie) {
  if (ie.type == views::ui::InteractionEvent::Type::Hover) {
    if (!clickable_) {
      return false;
    }
    int line_index = (ie.local.y - 4 + 2) / LINE_HEIGHT;
    if (line_index >= 0 && line_index < visible_lines()) {
      int index = *scroll_offset_ + line_index;
      if (index < static_cast<int>(lines_.size()) && !lines_[index].empty()) {
        hovered_index_ = index;
        return true;
      }
    }
    hovered_index_ = -1;
    return false;
  }

  if (ie.type == views::ui::InteractionEvent::Type::Scroll) {
    // Scroll up (positive delta) decreases the offset
    if (ie.scroll_delta > 0 && *scroll_offset_ > 0) {
      (*scroll_offset_)--;
      return true;
    }
    if (ie.scroll_delta < 0 && *scroll_offset_ < max_scroll()) {
      (*scroll_offset_)++;
      return true;
    }
    return false;
  }

  if (ie.type == views::ui::InteractionEvent::Type::Click &&
      ie.button == views::ui::MouseButton::Left) {
    if (clickable_ && on_line_clicked_ && hovered_index_ >= 0 &&
        hovered_index_ < static_cast<int>(lines_.size())) {
      on_line_clicked_(hovered_index_);
      return true;
    }
  }

  return false;
}
