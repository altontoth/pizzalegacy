/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/RecipePreview.h"

#include <filesystem>
#include "core/factories/PizzaFactory.h"
#include "core/food/Topping.h"
#include "core/food/ToppingSize.h"
#include "core/state/GameState.h"
#include "engine/gfx/Surface.h"
#include "resources/BobManager.h"
#include "resources/util/FileUtils.h"
#include "views/screens/pizza/HalfSizePizza.h"

using namespace pizza_design;

RecipePreview::RecipePreview(GameContext& context, GameState& state)
    : View(context, state), renderer_("bob/zutaten.bob", context.bob) {}

void RecipePreview::show_file(const std::filesystem::path& path) {
  const auto bob_size = core::food::topping_size_fn(*context_.bob);
  pizza_ = nullptr;
  const auto bytes = fileutils::read_bytes(path.string());
  if (PizzaFactory::read_summary(bytes)) {
    pizza_ = PizzaFactory::read_pizza(bytes, std::nullopt, bob_size);
  }
  mark_dirty();
}

void RecipePreview::render(const RenderContext& ctx) {
  if (!pizza_) {
    return;
  }
  // Clear the corners around the pizza so the box behind shows through.
  draw_half_size_pizza(*ctx.surface, {0, 0}, *pizza_, pizza_->toppings().size(),
                       *context_.vga, renderer_, gfx::TRANSPARENT);
}
