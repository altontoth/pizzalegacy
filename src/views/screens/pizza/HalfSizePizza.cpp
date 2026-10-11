/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/pizza/HalfSizePizza.h"

#include <algorithm>

#include "engine/geom/Camera.h"
#include "engine/gfx/Blit.h"
#include "views/base/ViewDefs.h"

namespace pizza_design {

void draw_half_size_pizza(gfx::Surface& dst, ViewPoint at,
                          const core::food::Pizza& pizza,
                          std::size_t topping_count, VgaManager& vga,
                          const BobRenderer& renderer,
                          gfx::ColorIndex outside_color) {
  gfx::Surface canvas(views::base::MAIN_PANEL_WIDTH,
                      views::base::MAIN_PANEL_HEIGHT);
  gfx::blit::blit_raw_with_palette(canvas, vga.get("gfx/back1"));
  const auto& toppings = pizza.toppings();
  const std::size_t count = std::min(topping_count, toppings.size());
  const Camera camera;
  for (std::size_t i = 0; i < count; ++i) {
    renderer.render(*toppings[i], canvas, camera);
  }

  const ViewRect pizza_rect{PIZZA_CENTER_X - PIZZA_RADIUS,
                            PIZZA_CENTER_Y - PIZZA_RADIUS, PIZZA_RADIUS * 2,
                            PIZZA_RADIUS * 2};
  gfx::blit::blit_half_with_palette(dst, canvas, pizza_rect, at);

  constexpr int r = half_size_pizza / 2;
  for (int y = 0; y < half_size_pizza; ++y) {
    for (int x = 0; x < half_size_pizza; ++x) {
      const int dx = x - r;
      const int dy = y - r;
      if ((dx * dx) + (dy * dy) > r * r) {
        dst.put(ViewPoint{at.x + x, at.y + y}, outside_color);
      }
    }
  }
}

} // namespace pizza_design
