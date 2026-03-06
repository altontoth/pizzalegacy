/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/DropdownDecoration.h"

#include <array>
#include <cstdint>

namespace views::ui::dropdowndecoration {

void draw_dropdown_decoration(gfx::Surface& s, int y_offset) {
  constexpr std::array<uint8_t, NUMBER_OF_BARS> bar_colors{
      9, 10, 12, 13, 14, 15, 14, 12, 10, 9,
  };

  int y = y_offset;
  for (auto color : bar_colors) {
    ViewRect bar{0, y, s.width(), LINE_HEIGHT};
    s.fill_rect(bar, color);
    y += LINE_HEIGHT;
  }
}

} // namespace views::ui::dropdowndecoration
