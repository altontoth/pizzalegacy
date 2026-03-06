/*
 * SPDX-FileCopyrightText: 2016 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/purchasing/PurchasingMain.h"

#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/supply/Supplier.h"
#include "gfx/Blit.h"
#include "resources/FontManager.h"

PurchasingMain::PurchasingMain(GameContext& context, GameState& state)
    : MainPanelView(context, state), status_controller_(state.status()) {
  set_up_input_panel();
  status_controller_.show_restaurant({.state = &state_,
                                      .rest = state_.active_restaurant(),
                                      .label = "EQUIPMENT"});
}

void PurchasingMain::tick() {}

void PurchasingMain::render(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back3"));
  ctx.surface->palette({60});

  uint8_t fill_color = ctx.surface->get({165, 80});
  //  Cover up the sticker at the bottom of the background:
  ViewRect rect_location = {96, 190, 208, 88};
  ctx.surface->fill_rect(rect_location, fill_color);

  context_.font->render_text_onto(*ctx.surface, "Furniture/",
                                  ViewPoint{162, 124}, FontFace::PIZ_NUMB, 8);
  context_.font->render_text_onto(*ctx.surface, "Equipment",
                                  ViewPoint{166, 148}, FontFace::PIZ_NUMB, 8);
}

void PurchasingMain::set_up_input_panel() {
  input_panel_->add_button(Icon::CHAIR, ButtonType::Square,
                           ButtonPosition::TopLeft, "Furniture", [this] {
                             Logger::debug("Furniture");
                             state_.ui().replace_top<CategoryOverviewState>(
                                 SupplierType::FURNITURE);
                           });

  input_panel_->add_button(Icon::OVEN, ButtonType::Square,
                           ButtonPosition::TopCenter, "Kitchenware", [this] {
                             Logger::debug("Kichtenware");
                             state_.ui().replace_top<CategoryOverviewState>(
                                 SupplierType::KITCHENWARE);
                           });
  input_panel_->add_button(Icon::PLANT, ButtonType::Square,
                           ButtonPosition::MiddleLeft, "Extras", [this] {
                             Logger::debug("Decorations/extras");
                             state_.ui().replace_top<CategoryOverviewState>(
                                 SupplierType::EXTRAS);
                           });
  input_panel_->add_button(Icon::FOOT, ButtonType::Square,
                           ButtonPosition::MiddleCenter, "Flooring", [this] {
                             Logger::debug("Floor");
                             state_.ui().replace_top<CategoryOverviewState>(
                                 SupplierType::FLOORING);
                           });
  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this] { state_.ui().pop(); });
}
