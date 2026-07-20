/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/stock/DepotIngredientCategoryView.h"

#include "core/city/CentralDepot.h"
#include "core/city/City.h"
#include "core/finance/Money.h"
#include "core/food/IngredientCategory.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "engine/FormatCompat.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/Alignment.h"
#include "engine/ui/InteractionEvent.h"
#include "factories/IngredientFactory.h"
#include "gfx/Blit.h"
#include "resources/BlitmapManager.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"
#include "views/ui/components/BeveledButton.h"

#include <algorithm>

DepotIngredientCategoryView::DepotIngredientCategoryView(
    GameContext& context, GameState& state, IngredientCategory category)
    : MainPanelView(context, state), status_controller_(state.status()),
      category_(category) {
  set_up_input_panel();
  status_controller_.show_city_default(
      {.state = &state_, .city = state_.active_city()});
}

void DepotIngredientCategoryView::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  ctx.surface->palette({60});
  if (show_priorities_) {
    draw_priorities(ctx);
  }
  else {
    draw_page_header(ctx);
    draw_ingredient_list(ctx);
  }
}

void DepotIngredientCategoryView::draw_icon(const RenderContext& ctx) {
  auto& blitmap1 = context_.blitmap->get<Blitmap1Icon>();
  Blitmap1Icon icon = core::food::CategoryRegistry::icon(category_);
  blitmap1.render_icon(icon, *ctx.surface, ViewPoint{42, 6});
}

void DepotIngredientCategoryView::draw_page_header(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back9"));
  draw_icon(ctx);
  const std::string title =
      "central depot " + std::string(state_.active_city()->name());
  context_.font->render_text_onto(*ctx.surface, title, ViewPoint{134, 12},
                                  FontFace::PIZ_NUMB, 8, AlignX::Left);
}

void DepotIngredientCategoryView::draw_ingredient_list(
    const RenderContext& ctx) {
  auto& depot =
      state_.active_city()->central_depot(state_.active_player()->id());

  const core::food::IngredientDemand demand_grams =
      state_.total_ingredient_need_for_player(state_.active_player()->id(),
                                              state_.active_city_id());

  const ViewPoint topleft{14, 30};
  const uint8_t text_color = 8;
  const int col_y = topleft.y;

  context_.font->render_text_onto(*ctx.surface, "qual", ViewPoint{176, col_y},
                                  FontFace::CSTANDAR, text_color,
                                  AlignX::RightEdge);
  context_.font->render_text_onto(*ctx.surface, "stoc", ViewPoint{226, col_y},
                                  FontFace::CSTANDAR, text_color,
                                  AlignX::RightEdge);
  context_.font->render_text_onto(*ctx.surface, "nee", ViewPoint{266, col_y},
                                  FontFace::CSTANDAR, text_color,
                                  AlignX::RightEdge);
  context_.font->render_text_onto(*ctx.surface, "value", ViewPoint{326, col_y},
                                  FontFace::CSTANDAR, text_color,
                                  AlignX::RightEdge);
  context_.font->render_text_onto(*ctx.surface, "lv", ViewPoint{356, col_y},
                                  FontFace::CSTANDAR, text_color,
                                  AlignX::RightEdge);

  auto [start_id, end_id] = core::food::CategoryRegistry::id_range(category_);

  const int name_x = topleft.x;
  int y = topleft.y + 20;
  constexpr int line_height = 12;

  Money depot_total_value{};

  for (int ing_id = start_id; ing_id < end_id; ing_id++) {
    auto* ingredient = IngredientFactory::create_ingredient(ing_id);
    if (!ingredient) {
      break;
    }

    const auto* stock = depot.get_ingredient_stock(ing_id);
    if (stock) {
      depot_total_value = depot_total_value + stock->total_value();
    }

    const int quality = stock ? stock->weighted_avg_quality : 0;
    const int supplies = stock ? stock->quantity / 1000 : 0;
    const int usage_grams = demand_grams[ing_id];
    const int need = (usage_grams + 999) / 1000;

    context_.font->render_text_onto(*ctx.surface, ingredient->name(),
                                    ViewPoint{name_x, y}, FontFace::CSTANDAR,
                                    text_color);

    constexpr int dark_red = 20;
    const int row_color = need > supplies ? dark_red : int{text_color};

    context_.font->render_text_onto(*ctx.surface, std::to_string(quality),
                                    ViewPoint{176, y}, FontFace::CSTANDAR,
                                    row_color, AlignX::RightEdge);
    context_.font->render_text_onto(*ctx.surface, std::to_string(supplies),
                                    ViewPoint{224, y}, FontFace::CSTANDAR,
                                    row_color, AlignX::RightEdge);
    context_.font->render_text_onto(*ctx.surface, std::to_string(need),
                                    ViewPoint{260, y}, FontFace::CSTANDAR,
                                    row_color, AlignX::RightEdge);
    if (stock) {
      context_.font->render_text_onto(
          *ctx.surface, compat::format("{:n}", stock->weighted_avg_cost),
          ViewPoint{326, y}, FontFace::CSTANDAR, text_color, AlignX::RightEdge);
      context_.font->render_text_onto(
          *ctx.surface, compat::format("{:n}", stock->last_price),
          ViewPoint{360, y}, FontFace::CSTANDAR, text_color, AlignX::RightEdge);
    }

    y += line_height;
  }

  context_.font->render_text_onto(*ctx.surface, "sum value", ViewPoint{96, 320},
                                  FontFace::CSTANDAR, text_color, AlignX::Left);
  context_.font->render_text_onto(
      *ctx.surface, compat::format("{}", depot_total_value),
      ViewPoint{312, 320}, FontFace::CSTANDAR, text_color, AlignX::RightEdge);
}

void DepotIngredientCategoryView::set_up_input_panel() {
  input_panel_->add_button(Icon::VEGETABLE, ButtonType::Square,
                           ButtonPosition::TopLeft, "Vegetables", [this]() {
                             show_priorities_ = false;
                             category_ = IngredientCategory::VEGETABLES;
                             mark_dirty();
                           });

  input_panel_->add_button(Icon::MEAT, ButtonType::Square,
                           ButtonPosition::TopCenter, "Meat", [this]() {
                             show_priorities_ = false;
                             category_ = IngredientCategory::MEAT;
                             mark_dirty();
                           });

  input_panel_->add_button(Icon::FRUIT, ButtonType::Square,
                           ButtonPosition::MiddleLeft, "Fruit", [this]() {
                             show_priorities_ = false;
                             category_ = IngredientCategory::FRUIT;
                             mark_dirty();
                           });

  input_panel_->add_button(Icon::CHEESE, ButtonType::Square,
                           ButtonPosition::MiddleCenter, "Cheese", [this]() {
                             show_priorities_ = false;
                             category_ = IngredientCategory::CHEESE;
                             mark_dirty();
                           });

  input_panel_->add_button(Icon::GLOBE, ButtonType::Square,
                           ButtonPosition::TopRight, "Export", [this]() {
                             show_priorities_ = false;
                             state_.ui().push<EuroMapState>(MapMode::DEPOT,
                                                            category_);
                           });

  input_panel_->add_button(Icon::EXCLAMATION_MARK, ButtonType::Square,
                           ButtonPosition::MiddleRight, "Priorities", [this]() {
                             show_priorities_ = true;
                             mark_dirty();
                           });

  input_panel_->add_button(
      Icon::BUY, ButtonType::Square, ButtonPosition::BottomLeft, "Buy",
      [this]() {
        state_.ui().replace_top<DepotIngredientCategoryViewState>(category_);
        state_.ui().push<DepotIngredientSupplierListState>(
            category_, IngredientScreenMode::CENTRAL_DEPOT_BUY);
      });

  input_panel_->add_button(
      Icon::SELL, ButtonType::Square, ButtonPosition::BottomCenter, "Sell",
      [this]() {
        state_.ui().replace_top<DepotIngredientCategoryViewState>(category_);
        state_.ui().push<DepotIngredientSupplierListState>(
            category_, IngredientScreenMode::CENTRAL_DEPOT_SELL);
      });

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this]() { state_.ui().pop(); });
}

void DepotIngredientCategoryView::draw_priorities(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back9"));

  constexpr int text_color = 8;
  context_.font->render_line_onto(*ctx.surface, "delivery priorities",
                                  ViewPoint{116, 18}, FontFace::PIZ_NUMB,
                                  text_color);

  auto& depot =
      state_.active_city()->central_depot(state_.active_player()->id());
  auto player_restaurants =
      state_.restaurants_for(state_.active_player()->id());

  std::vector<core::RestaurantId> restaurant_ids;
  restaurant_ids.reserve(player_restaurants.size());
  for (auto* r : player_restaurants) {
    restaurant_ids.push_back(r->id());
  }
  depot.sync_delivery_priorities(restaurant_ids);
  const auto& priorities = depot.delivery_priorities();

  constexpr int row_height = 24;
  const ViewRect start_box{76, 42, 240, 16};
  const ViewPoint label_pos{46, 46};
  const int slot_count = std::min(static_cast<int>(priorities.size()),
                                  core::city::max_delivery_priority_slots);

  for (int row_nr = 0; row_nr < slot_count; row_nr++) {
    const ViewRect this_box{start_box.x, start_box.y + (row_height * row_nr),
                            start_box.w, start_box.h};
    const ViewPoint this_label = label_pos + PxOffset{0, row_height * row_nr};

    context_.font->render_line_onto(*ctx.surface, std::to_string(row_nr + 1),
                                    this_label, FontFace::CSTANDAR, text_color);

    auto* restaurant = state_.restaurant(priorities[row_nr]);
    const std::string name = restaurant ? restaurant->name() : "???";

    const int captured_row = row_nr;
    const core::RestaurantId captured_id = priorities[row_nr];
    interactions_.create_frame_item(
        this_box, "row_" + std::to_string(row_nr),
        [this, captured_row, captured_id](views::ui::InteractionEvent ie) {
          if (ie.type == views::ui::InteractionEvent::Type::ButtonDown) {
            pressed_priority_row_ = captured_row;
            mark_dirty();
          }
          else if (ie.type == views::ui::InteractionEvent::Type::Click) {
            auto& d = state_.active_city()->central_depot(
                state_.active_player()->id());
            d.promote_to_top(captured_id);
            pressed_priority_row_.reset();
            mark_dirty();
          }
          else if (ie.type == views::ui::InteractionEvent::Type::Cancelled) {
            pressed_priority_row_.reset();
            mark_dirty();
          }
        });

    const bool is_pressed = (pressed_priority_row_ == captured_row);
    views::ui::components::draw_beveled_button(
        context_.font, context_.vga->get("gfx/permap"), *ctx.surface, this_box,
        name,
        is_pressed ? views::ui::components::ButtonState::Pressed
                   : views::ui::components::ButtonState::Normal);
  }
}
