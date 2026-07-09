/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/MissionTypeBox.h"

#include <algorithm>
#include "engine/geom/Spaces.h"
#include "resources/FontManager.h"

constexpr uint8_t DARK_BORDER = 10;
constexpr uint8_t GRAY = 14;
constexpr uint8_t WHITE = 15;
constexpr uint8_t TEXT_COLOR = 1;
constexpr int BORDER_MARGIN = 2;
constexpr PxOffset TEXT_PADDING{6, 6};

void MissionTypeBox::draw(Surface& screen, ViewPoint p, bool /*is_selected*/) {
  auto block = Surface(size_.w, size_.h, PaletteHandle{38}, DARK_BORDER);

  const int row_count = static_cast<int>(lines_.size());
  const int row_width = size_.w - (2 * BORDER_MARGIN);
  const int row_height = size_.h / row_count;

  int y = 0;
  for (const auto& line : lines_) {
    ViewRect row{BORDER_MARGIN, y, row_width, row_height};
    block.fill_rect(row, GRAY);
    context_.font->render_text_onto(block, line,
                                    ViewPoint{BORDER_MARGIN, y} + TEXT_PADDING,
                                    FontFace::PIZ_NUMB, TEXT_COLOR);
    y += row_height;
    if (y < size_.h) {
      block.fill_rect(ViewRect{BORDER_MARGIN, y - 1, row_width, 1}, WHITE);
    }
  }

  screen.blit_raw(block, p);
  height_ = block.height();
  width_ = block.width();
}

bool MissionTypeBox::on_interaction(views::ui::InteractionEvent ie) {
  if (ie.type != views::ui::InteractionEvent::Type::Click ||
      ie.button != views::ui::MouseButton::Left) {
    return false;
  }
  const int row_height = size_.h / static_cast<int>(lines_.size());
  int index = std::clamp(ie.local.y / row_height, 0,
                         static_cast<int>(lines_.size() - 1));
  if (on_line_clicked_) {
    on_line_clicked_(index);
  }
  return true;
}
