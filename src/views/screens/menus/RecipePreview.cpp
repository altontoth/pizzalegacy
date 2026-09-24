/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/RecipePreview.h"

#include <filesystem>
#include "core/factories/PizzaFactory.h"
#include "core/food/Topping.h"
#include "core/state/GameState.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/Blit.h"
#include "engine/gfx/Surface.h"
#include "resources/Bob.h"
#include "resources/BobManager.h"
#include "resources/BobSizeFn.h"
#include "resources/VgaManager.h"
#include "resources/util/FileUtils.h"
#include "views/base/ViewDefs.h"

using namespace pizza_design;

RecipePreview::RecipePreview(GameContext& context, GameState& state)
    : View(context, state), renderer_("bob/zutaten.bob", context.bob) {}

void RecipePreview::show_file(const std::filesystem::path& path) {
  Bob& zutaten = context_.bob->get("bob/zutaten.bob");
  const auto bob_size =
      make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten);
  pizza_ = nullptr;
  if (PizzaFactory::read_summary(path.string())) {
    pizza_ = PizzaFactory::create_pizza(fileutils::read_bytes(path.string()),
                                        std::nullopt, bob_size,
                                        state_.customer_taste_profiles());
  }
  mark_dirty();
}

void RecipePreview::render(const RenderContext& ctx) {
  if (!pizza_) {
    return;
  }
  Surface canvas(views::base::MAIN_PANEL_WIDTH, views::base::MAIN_PANEL_HEIGHT);
  gfx::blit::blit_raw_with_palette(canvas, context_.vga->get("gfx/back1"));
  for (const auto& topping : pizza_->toppings()) {
    renderer_.render(*topping, canvas, camera_);
  }

  const ViewRect pizza_rect{PIZZA_CENTER_X - PIZZA_RADIUS,
                            PIZZA_CENTER_Y - PIZZA_RADIUS, PIZZA_RADIUS * 2,
                            PIZZA_RADIUS * 2};
  gfx::blit::blit_half_with_palette(*ctx.surface, canvas, pizza_rect);

  // Clear the corners around the pizza so the box behind shows through.
  constexpr int r = size / 2;
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const int dx = x - r;
      const int dy = y - r;
      if ((dx * dx) + (dy * dy) > r * r) {
        ctx.surface->put(ViewPoint{x, y}, gfx::TRANSPARENT);
      }
    }
  }
}
