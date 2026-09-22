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
#include "engine/gfx/Blit.h"
#include "resources/FontManager.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/AlertPopup.h"

using namespace views::screens::depot;

DepotMain::DepotMain(GameContext& context, GameState& state, DepotMainState dms)
    : MainPanelView(context, state), status_controller_(state.status()),
      property_(dms.property) {
  set_up_input_panel();
  status_controller_.show_city_default(
      {.state = &state_, .city = state_.active_city()});
}

void DepotMain::tick() {
  if (auto stolen_cat = state_.active_city()
                            ->central_depot(state_.active_player()->id())
                            .take_stock_stolen()) {
    context_.popup->create<AlertPopup>(
        compat::format("Your central depot has been broken into! Your entire "
                       "stock of {} has been stolen!",
                       to_string(*stolen_cat)));
  }
}

void DepotMain::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back3"));
  ctx.surface->palette({15});

  TextStyle title{.color = 8, .align = TextAnchor::Middle};
  context_.font->render_text_onto(*ctx.surface, "central", ViewPoint{206, 126},
                                  FontFace::ASTANDAR, title);
  context_.font->render_text_onto(*ctx.surface, "stock", ViewPoint{206, 150},
                                  FontFace::ASTANDAR, title);

  auto& depot =
      state_.active_city()->central_depot(state_.active_player()->id());

  TextStyle ts{.color = 240};
  context_.font->render_text_onto(*ctx.surface, "capacity", ViewPoint{120, 210},
                                  FontFace::PIZ_NUMB, ts);
  context_.font->render_text_onto(*ctx.surface, "in use", ViewPoint{120, 232},
                                  FontFace::PIZ_NUMB, ts);
  context_.font->render_text_onto(*ctx.surface, "kg", ViewPoint{262, 210},
                                  FontFace::PIZ_NUMB, ts);
  context_.font->render_text_onto(*ctx.surface, "kg", ViewPoint{262, 232},
                                  FontFace::PIZ_NUMB, ts);

  ts.align = TextAnchor::End;
  context_.font->render_text_onto(*ctx.surface,
                                  compat::format("{}", depot.capacity()),
                                  ViewPoint{258, 210}, FontFace::PIZ_NUMB, ts);
  context_.font->render_text_onto(*ctx.surface,
                                  compat::format("{}", depot.used()),
                                  ViewPoint{258, 232}, FontFace::PIZ_NUMB, ts);
}

void DepotMain::set_up_input_panel() {
  auto open_category = [this](IngredientCategory category) {
    return [this, category] {
      state_.advance_time_blocks(1);
      state_.ui().replace_top<DepotIngredientCategoryViewState>(category);
    };
  };

  input_panel_->add_button(Icon::VEGETABLE, ButtonType::Square,
                           ButtonPosition::TopLeft, "Vegetables",
                           open_category(IngredientCategory::VEGETABLES));

  input_panel_->add_button(Icon::MEAT, ButtonType::Square,
                           ButtonPosition::TopCenter, "Meat",
                           open_category(IngredientCategory::MEAT));

  input_panel_->add_button(Icon::FRUIT, ButtonType::Square,
                           ButtonPosition::MiddleLeft, "Fruit",
                           open_category(IngredientCategory::FRUIT));

  input_panel_->add_button(Icon::CHEESE, ButtonType::Square,
                           ButtonPosition::MiddleCenter, "Cheese",
                           open_category(IngredientCategory::CHEESE));

  input_panel_->add_button(
      Icon::BOMB, ButtonType::Horizontal, ButtonPosition::BottomLeft, "Weapons",
      [this]() { state_.ui().push<DepotWeaponInterfaceState>(property_); });

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this]() { state_.ui().pop(); });
}
