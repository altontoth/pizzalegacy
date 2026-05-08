/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/purchasing/Demographics.h"

#include "core/city/Property.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "resources/BlitmapManager.h"
#include "views/ui/DemographicRenderer.h"

using namespace views::screens::purchasing;

Demographics::Demographics(GameContext& context, GameState& state)
    : RestaurantSubView(context, state, state.active_restaurant()->id()),
      restaurant_(state.active_restaurant()) {
  set_up_buttons();
}

void Demographics::tick() {}

void Demographics::set_up_buttons() {
  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this] { state_.ui().pop(); });
}

void Demographics::navigate_to_restaurant(core::RestaurantId new_id) {
  state_.active_restaurant(new_id);
  state_.ui().replace_top<PurchasingDemographicState>(SupplierType::FURNITURE);
}

void Demographics::render(const RenderContext& ctx) {
  ctx.surface->blit_raw(context_.vga->get("gfx/back9"));
  ctx.surface->palette({7});

  restaurant_->property().calculate_demographics(restaurant_);

  const auto popularity = restaurant_->calculate_furniture_appeal_popularity();

  views::ui::render_demographics(*ctx.surface, *context_.font,
                                 context_.blitmap->get2(), popularity,
                                 "Furniture");
}
