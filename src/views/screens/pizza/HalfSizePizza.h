/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>

#include "core/food/Pizza.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/ColorIndex.h"
#include "engine/gfx/Surface.h"
#include "resources/VgaManager.h"
#include "views/base/BobRenderer.h"
#include "views/screens/pizza/PizzaDesignHelpers.h"

namespace pizza_design {

// Width and height of a half size pizza.
inline constexpr int half_size_pizza = PIZZA_RADIUS;

// Draws a pizza's first topping_count toppings on its base at half size, and
// fills the area around the pizza with outside_color.
void draw_half_size_pizza(gfx::Surface& dst, ViewPoint at,
                          const core::food::Pizza& pizza,
                          std::size_t topping_count, VgaManager& vga,
                          const BobRenderer& renderer,
                          gfx::ColorIndex outside_color);

} // namespace pizza_design
