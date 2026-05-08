/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/stock/StockMain.h"

#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "engine/FormatCompat.h"
#include "engine/geom/Spaces.h"
#include "gfx/Blit.h"
#include "resources/FontManager.h"

using namespace views::screens::stock;

StockMain::StockMain(GameContext& context, GameState& state)
    : RestaurantSubView(context, state, state.active_restaurant()->id()),
      status_controller_(state.status()) {
  set_up_input_panel();
  status_controller_.show_restaurant(
      {.state = &state_, .rest = state_.active_restaurant(), .label = "STOCK"});
}

void StockMain::tick() {}

void StockMain::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back3"));
  ctx.surface->palette({59});

  context_.font->render_text_onto(*ctx.surface, "stocks", ViewPoint{168, 126},
                                  FontFace::ASTANDAR, 8);
  context_.font->render_text_onto(*ctx.surface, "depot", ViewPoint{170, 150},
                                  FontFace::ASTANDAR, 8);

  // Calculate depot info
  auto* restaurant = state_.active_restaurant();
  const auto& inventory = restaurant->inventory();

  int total_kg = 0;
  Money total_value{0};

  for (const auto& [ing_id, stock] : inventory.ingredients()) {
    total_kg += stock.quantity / 1000;
    total_value = total_value + Money{stock.weighted_avg_cost.cents *
                                      (stock.quantity / 1000)};
  }

  // Show kilograms and value
  std::string kg_text = compat::format("{} kg", total_kg);
  std::string value_text = compat::format("{:w}", total_value);

  TextStyle style{.color = 12, .character_spacing = 1};
  context_.font->render_text_onto(*ctx.surface, "kilogram", ViewPoint{130, 210},
                                  FontFace::PIZ_NUMB, style);
  context_.font->render_text_onto(*ctx.surface, "value", ViewPoint{130, 240},
                                  FontFace::PIZ_NUMB, style);

  style.color = 8;
  style.align = AlignX::Right;
  context_.font->render_text_onto(*ctx.surface, kg_text, ViewPoint{-106, 210},
                                  FontFace::PIZ_NUMB, style);
  context_.font->render_text_onto(*ctx.surface, value_text,
                                  ViewPoint{-110, 240}, FontFace::PIZ_NUMB,
                                  style);
}

void StockMain::navigate_to_restaurant(core::RestaurantId new_id) {
  state_.active_restaurant(new_id);
  state_.ui().replace_top<StockMainState>();
}

void StockMain::set_up_input_panel() {
  input_panel_->add_button(
      Icon::VEGETABLE, ButtonType::Square, ButtonPosition::TopLeft,
      "Vegetables", [this]() {
        state_.ui().replace_top<IngredientCategoryViewState>(
            IngredientCategory::VEGETABLES);
      });

  input_panel_->add_button(
      Icon::MEAT, ButtonType::Square, ButtonPosition::TopCenter, "Meat",
      [this]() {
        state_.ui().replace_top<IngredientCategoryViewState>(
            IngredientCategory::MEAT);
      });

  input_panel_->add_button(
      Icon::FRUIT, ButtonType::Square, ButtonPosition::MiddleLeft, "Fruit",
      [this]() {
        state_.ui().replace_top<IngredientCategoryViewState>(
            IngredientCategory::FRUIT);
      });

  input_panel_->add_button(
      Icon::CHEESE, ButtonType::Square, ButtonPosition::MiddleCenter, "Cheese",
      [this]() {
        state_.ui().replace_top<IngredientCategoryViewState>(
            IngredientCategory::CHEESE);
      });

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back", [this]() {
                             state_.advance_time_blocks(1);
                             state_.ui().pop();
                           });
}
