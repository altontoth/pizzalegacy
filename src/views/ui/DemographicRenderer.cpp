/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/DemographicRenderer.h"
#include "core/Demographics.h"
#include "resources/FontTypes.h"

namespace views::ui {

void render_demographics(gfx::Surface& surface, FontManager& font,
                         const Blitmap<Blitmap2Icon>& blitmap,
                         const Popularity& popularity,
                         const std::string& title) {
  // Render title at the top
  font.render_text_onto(surface, title, ViewPoint{140, 12}, FontFace::RUBY,
                        {.color = 8});

  font.render_text_onto(surface, "& customers", ViewPoint{92, 38},
                        FontFace::RUBY, {.color = 8});

  // Starting position for first demographic
  constexpr int start_x = 40;
  constexpr int icon_y = 278;
  constexpr int spacing = 50;
  constexpr int bar_bottom_y = 252;

  // Bar indicator icon (index 29 in blitmap2) - 2 pixels tall, 40 pixels wide
  const auto bar_icon = Blitmap2Icon::PIXEL_LINE;

  // Iterate through all demographics
  for (auto type : ALL_CONSUMER_TYPES) {
    int i = static_cast<int>(type);
    const int x = start_x + (i * spacing);

    // Get the popularity value for this demographic (can be negative)
    // but we only show 1 and up, and ignore anything above 150
    const int value = std::clamp(popularity[type], 0, 150);

    const auto demo_icon = static_cast<Blitmap2Icon>(i);
    blitmap.render_icon(demo_icon, surface, ViewPoint{x, icon_y});

    if (value >= 1) {
      const int bar_height_pixels = std::min(value, 100);

      // Bar starts above the icon and grows upward
      // Bar x position is same as icon, bar ends at icon_y and grows up
      const int bar_top_y = bar_bottom_y - bar_height_pixels;

      for (int bar_y = bar_bottom_y; bar_y > bar_top_y; bar_y -= 2) {
        blitmap.render_icon(bar_icon, surface, ViewPoint{x, bar_y});
      }

      // Render the number above the bar
      font.render_text_onto(surface, std::to_string(value),
                            ViewPoint{x + 6, bar_top_y - 16},
                            FontFace::CSTANDAR, {.color = 8});
    }
  }
}

} // namespace views::ui
