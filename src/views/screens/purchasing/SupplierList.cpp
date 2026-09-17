/*
 * SPDX-FileCopyrightText: 2016 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/purchasing/SupplierList.h"
#include "core/factories/SupplierFactory.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/supply/Supplier.h"

SupplierList::SupplierList(GameContext& context, GameState& state,
                           const SupplierListState& list_state)
    : CategoryOverviewScreen(context, state, list_state.supplier),
      current_page_(list_state.page) {

  category_titles[SupplierType::FURNITURE] = "Suppliers";
  category_titles[SupplierType::KITCHENWARE] = "Suppliers";
  category_titles[SupplierType::EXTRAS] = "Suppliers";
  category_titles[SupplierType::FLOORING] = "Suppliers";

  renderer_.emplace(SupplierFactory::all_suppliers_by_type(
                        supplier_type_, state.time().current_date().year()),
                    // Click callback - navigate to furniture/floor list
                    [this](const Supplier* supplier) {
                      if (auto* top = state_.ui().top_as<SupplierListState>()) {
                        top->page = current_page_;
                      }
                      if (supplier_type_ == SupplierType::FLOORING) {
                        state_.ui().push<FloorListState>(supplier->id());
                      }
                      else {
                        state_.ui().push<FurnitureListState>(supplier->id());
                      }
                    });

  set_up_input_panel();
}

void SupplierList::tick() {}

void SupplierList::draw_page_screen(const RenderContext& ctx) {
  interactions_.begin_frame();

  // Render the supplier list and page number using the helper
  renderer_->render_page(ctx, context_, interactions_, current_page_);
  renderer_->render_page_number(ctx, context_, current_page_);
}

void SupplierList::render(const RenderContext& ctx) {
  CategoryOverviewScreen::render(ctx);
  ctx.surface->palette({5});
}

void SupplierList::set_up_input_panel() {
  renderer_->setup_pagination_buttons(
      input_panel_.get(), current_page_, [this] { mark_dirty(); },
      [this] { state_.ui().pop(); });
}
