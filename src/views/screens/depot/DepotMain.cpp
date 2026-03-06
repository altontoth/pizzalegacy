/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/depot/DepotMain.h"

#include "core/city/CentralDepot.h"
#include "core/city/City.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "engine/FormatCompat.h"
#include "engine/geom/Spaces.h"
#include "gfx/Blit.h"
#include "resources/FontManager.h"

using namespace views::screens::depot;

DepotMain::DepotMain(GameContext& context, GameState& state, DepotMainState dms)
    : MainPanelView(context, state), status_controller_(state.status()),
      property_(dms.property) {
  set_up_input_panel();
  status_controller_.show_restaurant(
      {.state = &state_, .rest = state_.active_restaurant(), .label = "STOCK"});
}

void DepotMain::tick() {}

void DepotMain::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back3"));
  ctx.surface->palette({59});

  context_.font->render_text_onto(*ctx.surface, "central", ViewPoint{168, 126},
                                  FontFace::ASTANDAR, 8);
  context_.font->render_text_onto(*ctx.surface, "stock", ViewPoint{170, 150},
                                  FontFace::ASTANDAR, 8);

  // Calculate depot info from central depot
  auto& depot =
      state_.active_city()->central_depot(state_.active_player()->id().value);
  const auto& stocks = depot.all_stocks();

  int total_kg = 0;
  Money total_value{0};

  for (const auto& stock : stocks) {
    total_kg += stock.quantity / 1000;
    total_value = total_value + Money{stock.weighted_avg_cost.cents *
                                      (stock.quantity / 1000)};
  }

  // Show kilograms and value
  std::string kg_text = compat::format("{} kg", total_kg);
  std::string value_text = compat::format("{:w}", total_value);

  context_.font->render_text_onto(*ctx.surface, "kilogram", ViewPoint{130, 210},
                                  FontFace::PIZ_NUMB, 12);
  context_.font->render_text_onto(*ctx.surface, "value", ViewPoint{130, 240},
                                  FontFace::PIZ_NUMB, 12);

  context_.font->render_text_onto(*ctx.surface, kg_text, ViewPoint{-106, 210},
                                  FontFace::PIZ_NUMB, 8, AlignX::Right);
  context_.font->render_text_onto(*ctx.surface, value_text,
                                  ViewPoint{-110, 240}, FontFace::PIZ_NUMB, 8,
                                  AlignX::Right);
}

void DepotMain::set_up_input_panel() {
  input_panel_->add_button(
      Icon::VEGETABLE, ButtonType::Square, ButtonPosition::TopLeft,
      "Vegetables", [this]() {
        state_.ui().replace_top<IngredientCategoryViewState>(
            IngredientCategory::VEGETABLES,
            IngredientScreenMode::CENTRAL_DEPOT);
      });

  input_panel_->add_button(
      Icon::MEAT, ButtonType::Square, ButtonPosition::TopCenter, "Meat",
      [this]() {
        state_.ui().replace_top<IngredientCategoryViewState>(
            IngredientCategory::MEAT, IngredientScreenMode::CENTRAL_DEPOT);
      });

  input_panel_->add_button(
      Icon::FRUIT, ButtonType::Square, ButtonPosition::MiddleLeft, "Fruit",
      [this]() {
        state_.ui().replace_top<IngredientCategoryViewState>(
            IngredientCategory::FRUIT, IngredientScreenMode::CENTRAL_DEPOT);
      });

  input_panel_->add_button(
      Icon::CHEESE, ButtonType::Square, ButtonPosition::MiddleCenter, "Cheese",
      [this]() {
        state_.ui().replace_top<IngredientCategoryViewState>(
            IngredientCategory::CHEESE, IngredientScreenMode::CENTRAL_DEPOT);
      });

  input_panel_->add_button(Icon::BOMB, ButtonType::Horizontal,
                           ButtonPosition::BottomLeft, "Weapons", [this]() {
                             state_.ui().replace_top<DepotWeaponInterfaceState>(
                                 property_);
                           });

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this]() { state_.ui().pop(); });
}
