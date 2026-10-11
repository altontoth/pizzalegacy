/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// The read-only Ingredients and Info views of the current pizza.

#include "core/Demographics.h"
#include "core/city/Property.h"
#include "core/factories/PizzaFactory.h"
#include "core/finance/Money.h"
#include "core/food/Ingredient.h"
#include "core/food/Topping.h"
#include "core/food/ToppingSize.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/gfx/Blit.h"
#include "engine/gfx/Draw.h"
#include "engine/ui/TextAnchor.h"
#include "views/screens/pizza/PizzaDesign.h"
#include "views/ui/widgets/StatBar.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace core::food;

namespace {
constexpr int weight_x = 134;
constexpr int name_x = 140;
constexpr int compare_name_x = 50;
constexpr int compare_ours_x = 236;
constexpr int compare_cook_book_x = 340;
} // namespace

void PizzaDesign::draw_ingredients(const RenderContext& ctx) const {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back4"));

  const auto* pizza = current_pizza();
  if (!pizza) {
    Logger::warn("No current pizza!");
    return;
  }

  constexpr uint8_t text_color = 5;
  context_.font->render_text_onto(*ctx.surface, "recipe " + pizza->name(),
                                  ViewPoint{40, 40}, FontFace::ASTANDAR,
                                  text_color);

  const Pizza* cook_book = nullptr;
  if (bake_ && bake_->reference_id && state_.rules().cook_book) {
    if (!bake_->cook_book) {
      bake_->cook_book = PizzaFactory::read_pizza(
          *bake_->reference_id, core::food::topping_size_fn(*context_.bob));
    }
    cook_book = bake_->cook_book.get();
  }

  struct Row {
    std::string name;
    int ours{0};
    int cook_book{0};
  };
  // In the cook book's order, then whatever else is on our pizza.
  std::vector<Row> rows;
  const auto row = [&rows](const IngredientGrams& grams) -> Row& {
    const auto name = grams.ingredient->name();
    auto it = std::ranges::find(rows, name, &Row::name);
    if (it == rows.end()) {
      return rows.emplace_back(Row{.name = name});
    }
    return *it;
  };
  Row total{.name = "TOTAL WEIGHT"};
  if (cook_book) {
    for (const auto& grams : cook_book->ingredient_grams()) {
      row(grams).cook_book = grams.grams;
      total.cook_book += grams.grams;
    }
  }
  const auto ours = pizza->ingredient_grams();
  for (const auto& grams : ours) {
    row(grams).ours = grams.grams;
    total.ours += grams.grams;
  }
  const auto ingredients_used = ours.size();

  const int ours_x = cook_book ? compare_ours_x : weight_x;
  const int names_x = cook_book ? compare_name_x : name_x;
  constexpr int row_height = 14;
  int y = 70;
  const auto draw_text = [&](const std::string& text, int x, TextAnchor align) {
    context_.font->render_text_onto(*ctx.surface, text, ViewPoint{x, y},
                                    FontFace::CSTANDAR, text_color, align);
  };
  const auto draw_row = [&](const Row& r) {
    draw_text(r.name, names_x, TextAnchor::Start);
    draw_text(std::to_string(r.ours) + " G", ours_x, TextAnchor::End);
    if (cook_book) {
      draw_text(std::to_string(r.cook_book) + " G", compare_cook_book_x,
                TextAnchor::End);
    }
    y += row_height;
  };

  if (cook_book) {
    draw_text("YOURS", ours_x, TextAnchor::End);
    draw_text("COOK BOOK", compare_cook_book_x, TextAnchor::End);
    y += row_height;
  }
  draw_row(total);
  for (const auto& r : rows) {
    draw_row(r);
  }

  context_.font->render_text_onto(
      *ctx.surface, std::to_string(ingredients_used) + " ingredients used",
      ViewPoint{130, 306}, FontFace::CSTANDAR, text_color);
}

void PizzaDesign::draw_pizza_info(const RenderContext& ctx) const {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back4"));

  const Pizza* pizza = current_pizza();
  if (!pizza) {
    Logger::warn("No current pizza!");
    return;
  }

  uint8_t text_color = 5;

  context_.font->render_text_onto(*ctx.surface, "Info: " + pizza->name(),
                                  ViewPoint{40, 40}, FontFace::ASTANDAR,
                                  text_color);

  // Let's start by drawing the table lines
  uint8_t line_color = 27;
  gfx::draw::line(*ctx.surface, {150 / 2, 154 / 2}, {652 / 2, 154 / 2},
                  line_color);
  gfx::draw::line(*ctx.surface, {328 / 2, 122 / 2}, {328 / 2, 372 / 2},
                  line_color);
  gfx::draw::line(*ctx.surface, {496 / 2, 122 / 2}, {496 / 2, 372 / 2},
                  line_color);

  context_.font->render_text_onto(*ctx.surface, "here",
                                  ViewPoint{372 / 2, 114 / 2},
                                  FontFace::PIZ_NUMB, text_color);
  context_.font->render_text_onto(*ctx.surface, "all",
                                  ViewPoint{516 / 2, 114 / 2},
                                  FontFace::PIZ_NUMB, text_color);

  // Get the recipe ID if this is a saved recipe
  std::optional<core::RecipeId> recipe_id;
  if (active_key_ && std::holds_alternative<core::RecipeId>(*active_key_)) {
    recipe_id = std::get<core::RecipeId>(*active_key_);
  }

  // Recipes belong to the player, so there need not be a restaurant in view.
  auto* restaurant = state_.active_restaurant();
  auto* city = state_.active_city();

  // Compute live ingredient cost for a pizza at a given restaurant.
  auto compute_pizza_cost = [&](const auto* rest) {
    Money cost{0};
    for (const auto& topping : pizza->toppings()) {
      cost += rest->ingredient_price(topping->ingredient()->type()) *
              topping->weight() / 1000;
    }
    return cost;
  };

  Money pizza_cost{0};
  if (restaurant) {
    pizza_cost = compute_pizza_cost(restaurant);
  }

  // Calculate values for "here" (this restaurant)
  Money here_price{0};
  int here_sales = 0;
  Money here_costs{0};
  Money here_profit{0};
  bool on_current_menu = false;

  if (recipe_id && restaurant) {
    // Check if pizza is on this restaurant's menu
    if (const auto* menu_entry = restaurant->menu().find(*recipe_id)) {
      on_current_menu = true;
      here_price = menu_entry->price;

      // Get sales data from yesterday
      const auto& stats = restaurant->stats();
      if (auto it = stats.yesterday.pizza_sales.find(*recipe_id);
          it != stats.yesterday.pizza_sales.end()) {
        here_sales = it->second.count_sold;
        here_costs = it->second.ingredient_cost;
        here_profit = it->second.profit;
      }
    }
  }

  // Calculate values for "all"
  Money all_price{0};
  int all_sales = 0;
  Money all_costs{0};
  Money all_profit{0};
  int restaurants_with_pizza = 0;

  if (recipe_id) {
    auto* player = competing_player();

    for (auto* rest : state_.restaurants_for(player->id())) {
      if (rest->property().city().id() != city->id()) {
        continue;
      }

      if (const auto* menu_entry = rest->menu().find(*recipe_id)) {
        restaurants_with_pizza++;
        all_price = all_price + menu_entry->price;
        all_costs = all_costs + compute_pizza_cost(rest);

        if (auto it = rest->stats().yesterday.pizza_sales.find(*recipe_id);
            it != rest->stats().yesterday.pizza_sales.end()) {
          all_sales += it->second.count_sold;
          all_profit = all_profit + it->second.profit;
        }
      }
    }

    if (restaurants_with_pizza > 0) {
      all_price = Money{all_price.cents / restaurants_with_pizza};
      all_costs = Money{all_costs.cents / restaurants_with_pizza};
    }
  }

  int label_x = 75;
  int label_y = 85;
  const int right_edge = ctx.surface->width();
  const int here_x = right_edge - 145; // "here" column
  const int all_x = right_edge - 65;   // "all" column (further left)
  text_color = 6;

  // costs
  context_.font->render_text_onto(*ctx.surface, "costs",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, text_color);

  // Show calculated cost in "here" column
  if (restaurant) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", pizza_cost),
        ViewPoint{here_x, label_y}, FontFace::PIZ_NUMB, text_color,
        TextAnchor::End);
  }

  if (restaurants_with_pizza > 0) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", all_costs),
        ViewPoint{all_x, label_y}, FontFace::PIZ_NUMB, text_color,
        TextAnchor::End);
  }
  label_y += 21;

  // price - only show if on menu
  context_.font->render_text_onto(*ctx.surface, "price",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, text_color);

  if (on_current_menu) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", here_price),
        ViewPoint{here_x, label_y}, FontFace::PIZ_NUMB, text_color,
        TextAnchor::End);
  }

  if (restaurants_with_pizza > 0) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", all_price),
        ViewPoint{all_x, label_y}, FontFace::PIZ_NUMB, text_color,
        TextAnchor::End);
  }
  label_y += 21;

  // sales (count)
  context_.font->render_text_onto(*ctx.surface, "sales",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, text_color);

  if (on_current_menu) {
    context_.font->render_text_onto(
        *ctx.surface, std::to_string(here_sales), ViewPoint{here_x, label_y},
        FontFace::PIZ_NUMB, text_color, TextAnchor::End);
  }

  if (restaurants_with_pizza > 0) {
    context_.font->render_text_onto(
        *ctx.surface, std::to_string(all_sales), ViewPoint{all_x, label_y},
        FontFace::PIZ_NUMB, text_color, TextAnchor::End);
  }
  label_y += 21;

  // profit
  context_.font->render_text_onto(*ctx.surface, "profit",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, text_color);

  if (on_current_menu) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", here_profit),
        ViewPoint{here_x, label_y}, FontFace::PIZ_NUMB, text_color,
        TextAnchor::End);
  }

  if (restaurants_with_pizza > 0) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", all_profit),
        ViewPoint{all_x, label_y}, FontFace::PIZ_NUMB, text_color,
        TextAnchor::End);
  }

  context_.font->render_text_onto(*ctx.surface, "popularity",
                                  ViewPoint{154, 200}, FontFace::PIZ_NUMB,
                                  text_color);
  constexpr int max_rows_per_column = 3;
  int y_start = 232;
  int x_start = 14;
  // Move this much sideways for the second column
  constexpr int column_offset = 172;
  constexpr int row_offset = 30;
  Popularity p = pizza->popularity();
  StatBar statbar(context_.blitmap, context_.font);
  int y_offset = 0;
  for (std::size_t i = 0; i < static_cast<std::size_t>(ConsumerType::Count);
       ++i) {
    auto d = static_cast<ConsumerType>(i);
    auto bar = statbar.create_stat_bar(to_string(d), static_cast<int>(p[d]));
    ctx.surface->blit_masked(bar, ViewPoint{x_start, y_start + y_offset});
    y_offset += row_offset;
    if (i + 1 == max_rows_per_column) {
      x_start += column_offset;
      y_offset = 0;
    }
  }
}
