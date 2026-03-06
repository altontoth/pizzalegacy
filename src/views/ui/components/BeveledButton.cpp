/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/components/BeveledButton.h"

#include <cstdlib>

#include "gfx/Draw.h"
#include "resources/FontManager.h"

namespace views::ui::components {

namespace {
constexpr ViewRect permap_button_src = {0, 202, 176, 16};

inline void blit_tiled(Surface& dst, const Surface& atlas, ViewRect src,
                       ViewRect dst_area) {
  // Tile src across dst_area, clipping the last tiles as needed.
  for (int dy = 0; dy < dst_area.h; dy += src.h) {
    int h = std::min(src.h, dst_area.h - dy);
    for (int dx = 0; dx < dst_area.w; dx += src.w) {
      int w = std::min(src.w, dst_area.w - dx);
      ViewRect s = {src.x, src.y, w, h};
      ViewPoint d = {dst_area.x + dx, dst_area.y + dy};
      dst.blit(atlas, s, d);
    }
  }
}
} // namespace

void draw_beveled_button(FontManager* font, const gfx::Surface& permap,
                         Surface& dst, const ViewRect& box,
                         std::string_view label, ButtonState state,
                         const BeveledButtonStyle& style) {

  // Bevel: swap light/dark when pressed
  const uint8_t light =
      (state == ButtonState::Pressed) ? style.dark_color : style.light_color;
  const uint8_t dark =
      (state == ButtonState::Pressed) ? style.light_color : style.dark_color;
  gfx::draw::beveled_box(dst, box, light, dark, /*thickness=*/2);

  // Background: tile the permap strip to fill the box.
  blit_tiled(dst, permap, permap_button_src, box.inset(2, 2));

  const PxOffset text_pos{6, 4};

  font->render_line_onto(dst, label, box.point() + text_pos, style.font_face,
                         style.text_color);
}

} // namespace views::ui::components
