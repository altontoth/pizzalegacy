/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/TextListBox.h"

#include <algorithm>
#include <utility>
#include "engine/geom/Spaces.h"
#include "engine/gfx/Draw.h"
#include "resources/FontManager.h"

constexpr int LINE_HEIGHT = 18;
constexpr int TEXT_COLOR = 1;
constexpr uint8_t PLACEHOLDER_COLOR = 13;
constexpr uint8_t ACTIVE_ROW = 13;
constexpr uint8_t INACTIVE_ROW = 14;
constexpr uint8_t FOCUS_COLOR = 29;

void TextListBox::set_lines(std::vector<std::string> lines, bool clickable,
                            std::string placeholder) {
  lines_ = std::move(lines);
  placeholder_ = std::move(placeholder);
  clickable_ = clickable;
  cursor_.active = -1;
  *scroll_offset_ = std::clamp(*scroll_offset_, 0, max_scroll());
}

int TextListBox::visible_lines() const { return size_.h / LINE_HEIGHT; }

int TextListBox::max_scroll() const {
  return std::max(0, static_cast<int>(lines_.size()) - visible_lines());
}

ScrollBar::Extent TextListBox::scroll_extent() const {
  return {.offset = *scroll_offset_,
          .visible = visible_lines(),
          .total = static_cast<int>(lines_.size())};
}

// The clickable line under the given widget-local y, or -1.
int TextListBox::line_at(int local_y) const {
  const int row = (local_y - 4 + 2) / LINE_HEIGHT;
  if (row < 0 || row >= visible_lines()) {
    return -1;
  }
  const int index = *scroll_offset_ + row;
  if (index >= static_cast<int>(lines_.size()) || lines_[index].empty()) {
    return -1;
  }
  return index;
}

void TextListBox::draw(Surface& screen, ViewPoint p, bool is_selected) {
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

    if (clickable_ && line_index == cursor_.active) {
      ViewRect highlight_rect{0, y, size_.w, LINE_HEIGHT};
      block.fill_rect(highlight_rect, is_selected ? ACTIVE_ROW : INACTIVE_ROW);
    }

    context_.font->render_text_onto(block, lines_[line_index], ViewPoint{6, y},
                                    FontFace::PIZ_NUMB, TEXT_COLOR);
    y += LINE_HEIGHT;
  }

  if (lines_.empty() && !placeholder_.empty()) {
    context_.font->render_text_onto(block, placeholder_, ViewPoint{6, 4},
                                    FontFace::PIZ_NUMB, PLACEHOLDER_COLOR);
  }

  if (scroll_thumb_) {
    scroll_bar_.draw(block, ViewRect{{0, 0}, size_}, scroll_extent());
  }

  screen.blit_raw(block, p);
  if (is_selected) {
    gfx::draw::box(screen, ViewRect{p.x, p.y, size_.w, size_.h}, FOCUS_COLOR,
                   2);
  }
  height_ = block.height();
}

void TextListBox::handle_action() {
  if (clickable_ && on_line_clicked_ && cursor_.active >= 0 &&
      cursor_.active < static_cast<int>(lines_.size())) {
    on_line_clicked_(cursor_.active);
  }
}

bool TextListBox::on_adjust(AdjustAxis axis, int delta) {
  if (axis != AdjustAxis::Vertical) {
    return false;
  }
  if (!clickable_) {
    // Display-only list: Up/Down scrolls.
    const int target = std::clamp(*scroll_offset_ + delta, 0, max_scroll());
    if (target == *scroll_offset_) {
      return false;
    }
    *scroll_offset_ = target;
    return true;
  }
  return cursor_.step(delta, static_cast<int>(lines_.size()), scroll_offset_,
                      visible_lines());
}

bool TextListBox::on_interaction(views::ui::InteractionEvent ie) {
  if (scroll_thumb_ &&
      scroll_bar_.on_interaction(ie, ie.local, ViewRect{{0, 0}, size_},
                                 scroll_extent(), *scroll_offset_)) {
    return true;
  }
  // Consume the press; mouse behavior happens on Click.
  if (ie.type == views::ui::InteractionEvent::Type::ButtonDown) {
    return true;
  }
  if (ie.type == views::ui::InteractionEvent::Type::Hover) {
    if (!clickable_) {
      return false;
    }
    const int index = line_at(ie.local.y);
    if (index < 0) {
      return false;
    }
    cursor_.active = index;
    return true;
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
    const int index = line_at(ie.local.y);
    if (clickable_ && on_line_clicked_ && index >= 0) {
      cursor_.active = index;
      on_line_clicked_(index);
      return true;
    }
  }

  return false;
}
