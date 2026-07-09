/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// The read-only Ingredients and Info views of the current pizza.

#include "core/Demographics.h"
#include "core/city/Property.h"
#include "core/finance/Money.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/ui/Alignment.h"
#include "gfx/Blit.h"
#include "gfx/Draw.h"
#include "views/screens/pizza/PizzaDesign.h"
#include "views/ui/widgets/StatBar.h"

#include <string>
#include <unordered_map>

using namespace core::food;

void PizzaDesign::draw_ingredients(const RenderContext& ctx) const {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back4"));

  const auto* pizza = current_pizza();
  if (!pizza) {
    Logger::warn("No current pizza!");
    return;
  }

  constexpr int text_color = 5;
  context_.font->render_text_onto(*ctx.surface, "recipe " + pizza->name(),
                                  ViewPoint{40, 40}, FontFace::ASTANDAR,
                                  text_color);

  std::unordered_map<std::string, uint32_t> summary_map;
  uint32_t total_weight = 0;
  for (const auto& topping : pizza->toppings()) {
    std::string ingredient_name = topping->ingredient()->name();
    auto weight = topping->weight();
    summary_map[ingredient_name] += weight;
    total_weight += weight;
  }
  int weight_x = -250;
  int name_x = 140;
  int y = 70;

  context_.font->render_text_onto(*ctx.surface, "TOTAL WEIGHT",
                                  ViewPoint{name_x, y}, FontFace::CSTANDAR,
                                  text_color);
  context_.font->render_text_onto(
      *ctx.surface, std::to_string(total_weight) + " G", ViewPoint{weight_x, y},
      FontFace::CSTANDAR, text_color, AlignX::Right);

  for (const auto& item : summary_map) {
    y += 14;
    context_.font->render_text_onto(*ctx.surface, item.first,
                                    ViewPoint{name_x, y}, FontFace::CSTANDAR,
                                    text_color);
    context_.font->render_text_onto(
        *ctx.surface, std::to_string(item.second) + " G",
        ViewPoint{weight_x, y}, FontFace::CSTANDAR, text_color, AlignX::Right);
  }

  context_.font->render_text_onto(
      *ctx.surface, std::to_string(summary_map.size()) + " ingredients used",
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

  int text_color = 5;

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

  // Get data from current restaurant
  auto* restaurant = state_.active_restaurant();
  const auto& menu = restaurant->menu();
  const auto& stats = restaurant->stats();
  auto* city = state_.active_city();

  // Compute live ingredient cost for a pizza at a given restaurant.
  // Uses weighted-average vendor prices, falls back to city base prices.
  auto compute_pizza_cost = [&](const auto* rest) {
    const auto& inv = rest->inventory();
    Money cost{0};
    for (const auto& topping : pizza->toppings()) {
      auto type = topping->ingredient()->type();
      int weight_grams = topping->weight();
      const auto* stock = inv.get_ingredient_stock(type);
      Money price_per_1000g;
      if (stock && stock->quantity > 0 && stock->weighted_avg_cost > 0_usd) {
        price_per_1000g = stock->weighted_avg_cost;
      }
      else {
        price_per_1000g = city->ingredient_base_price(type);
      }
      cost += price_per_1000g * weight_grams / 1000;
    }
    return cost;
  };

  Money pizza_cost = compute_pizza_cost(restaurant);

  // Calculate values for "here" (this restaurant)
  Money here_price{0};
  int here_sales = 0;
  Money here_costs{0};
  Money here_profit{0};
  bool on_current_menu = false;

  if (recipe_id) {
    // Check if pizza is on this restaurant's menu
    if (const auto* menu_entry = menu.find(*recipe_id)) {
      on_current_menu = true;
      here_price = menu_entry->price;

      // Get sales data from yesterday
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
    auto* player = state_.active_player();

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
  int here_x_offset = -220; // "here" column
  int all_x_offset = -140;  // "all" column (further left)
  text_color = 6;

  // costs
  context_.font->render_text_onto(*ctx.surface, "costs",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, text_color);

  // Show calculated cost in "here" column
  context_.font->render_text_onto(
      *ctx.surface, compat::format("{:n}", pizza_cost),
      ViewPoint{label_x + here_x_offset, label_y}, FontFace::PIZ_NUMB,
      text_color, AlignX::Right);

  if (restaurants_with_pizza > 0) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", all_costs),
        ViewPoint{label_x + all_x_offset, label_y}, FontFace::PIZ_NUMB,
        text_color, AlignX::Right);
  }
  label_y += 21;

  // price - only show if on menu
  context_.font->render_text_onto(*ctx.surface, "price",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, text_color);

  if (on_current_menu) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", here_price),
        ViewPoint{label_x + here_x_offset, label_y}, FontFace::PIZ_NUMB,
        text_color, AlignX::Right);
  }

  if (restaurants_with_pizza > 0) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", all_price),
        ViewPoint{label_x + all_x_offset, label_y}, FontFace::PIZ_NUMB,
        text_color, AlignX::Right);
  }
  label_y += 21;

  // sales (count)
  context_.font->render_text_onto(*ctx.surface, "sales",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, text_color);

  if (on_current_menu) {
    context_.font->render_text_onto(*ctx.surface, std::to_string(here_sales),
                                    ViewPoint{label_x + here_x_offset, label_y},
                                    FontFace::PIZ_NUMB, text_color,
                                    AlignX::Right);
  }

  if (restaurants_with_pizza > 0) {
    context_.font->render_text_onto(*ctx.surface, std::to_string(all_sales),
                                    ViewPoint{label_x + all_x_offset, label_y},
                                    FontFace::PIZ_NUMB, text_color,
                                    AlignX::Right);
  }
  label_y += 21;

  // profit
  context_.font->render_text_onto(*ctx.surface, "profit",
                                  ViewPoint{label_x, label_y},
                                  FontFace::PIZ_NUMB, text_color);

  if (on_current_menu) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", here_profit),
        ViewPoint{label_x + here_x_offset, label_y}, FontFace::PIZ_NUMB,
        text_color, AlignX::Right);
  }

  if (restaurants_with_pizza > 0) {
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:n}", all_profit),
        ViewPoint{label_x + all_x_offset, label_y}, FontFace::PIZ_NUMB,
        text_color, AlignX::Right);
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
