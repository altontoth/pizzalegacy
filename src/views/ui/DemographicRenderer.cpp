/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/DemographicRenderer.h"
#include <algorithm>
#include <string>
#include "core/Demographics.h"
#include "engine/gfx/ColorIndex.h"
#include "engine/ui/TextAnchor.h"
#include "resources/FontTypes.h"

namespace views::ui {

void render_demographics(gfx::Surface& surface, FontManager& font,
                         const Blitmap<Blitmap2Icon>& blitmap,
                         const Popularity& popularity,
                         const std::string& title) {
  constexpr int title_center_x = 191;
  constexpr gfx::ColorIndex text_color = 12;
  const TextStyle title_style{.color = text_color, .align = TextAnchor::Middle};
  font.render_text_onto(surface, title, ViewPoint{title_center_x, 14},
                        FontFace::RUBY, title_style);
  font.render_text_onto(surface, " & customers", ViewPoint{title_center_x, 38},
                        FontFace::RUBY, title_style);

  constexpr int column_spacing = 52;
  constexpr int icon_x = 38;
  constexpr int icon_y = 278;
  constexpr int bar_x = 46;
  constexpr int bar_bottom_y = 250;
  constexpr int bar_top_limit_y = 104;
  constexpr int number_right_x = 76;
  constexpr int number_above_bar = 10;

  for (auto type : ALL_CONSUMER_TYPES) {
    const int i = static_cast<int>(type);
    const int column = i * column_spacing;

    const auto demo_icon = static_cast<Blitmap2Icon>(i);
    blitmap.render_icon(demo_icon, surface, ViewPoint{icon_x + column, icon_y});

    const int value = popularity[type];
    if (value <= 0) {
      continue;
    }

    // One two-pixel row per two points, going up from the icon.
    const int bar_top_y =
        std::max(bar_bottom_y - ((value / 2) * 2), bar_top_limit_y);
    for (int bar_y = bar_bottom_y; bar_y > bar_top_y; bar_y -= 2) {
      blitmap.render_icon(Blitmap2Icon::PIXEL_LINE, surface,
                          ViewPoint{bar_x + column, bar_y});
    }

    font.render_text_onto(
        surface, std::to_string(value),
        ViewPoint{number_right_x + column, bar_top_y - number_above_bar},
        FontFace::CSTANDAR, {.color = text_color, .align = TextAnchor::End});
  }
}

} // namespace views::ui
