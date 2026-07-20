/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/stock/IngredientCategoryView.h"

#include "core/state/UIState.h"

#include "core/food/IngredientCategory.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/Alignment.h"
#include "factories/IngredientFactory.h"
#include "gfx/Blit.h"
#include "gfx/Draw.h"
#include "resources/BlitmapManager.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"
#include "views/ui/PopupManager.h"
#include "views/ui/components/BeveledButton.h"
#include "views/ui/popups/AlertPopup.h"

#include <algorithm>

IngredientCategoryView::IngredientCategoryView(GameContext& context,
                                               GameState& state,
                                               IngredientCategory category)
    : StockMain(context, state), category_(category) {
  set_up_input_panel();
  status_controller_.show_restaurant(
      {.state = &state_, .rest = state_.active_restaurant(), .label = "STOCK"});
}

void IngredientCategoryView::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  ctx.surface->palette({60});
  draw_page_header(ctx);
  draw_ingredient_list(ctx);
}

void IngredientCategoryView::draw_icon(const RenderContext& ctx) {
  auto& blitmap1 = context_.blitmap->get<Blitmap1Icon>();
  Blitmap1Icon icon = core::food::CategoryRegistry::icon(category_);
  blitmap1.render_icon(icon, *ctx.surface, ViewPoint{42, 6});
}

void IngredientCategoryView::draw_page_header(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back9"));
  draw_icon(ctx);
  const std::string title =
      "supplies " + std::string(core::food::CategoryRegistry::name(category_));
  context_.font->render_text_onto(*ctx.surface, title, ViewPoint{138, 22},
                                  FontFace::PIZ_NUMB, 8, AlignX::Left);
}

void IngredientCategoryView::draw_ingredient_list(const RenderContext& ctx) {
  auto* restaurant = state_.active_restaurant();
  const auto& inventory = restaurant->inventory();

  auto* player = state_.active_player();
  const core::food::IngredientDemand& demand_grams =
      restaurant->daily_ingredient_demand_cache();

  const ViewPoint topleft{52, 44};
  const uint8_t text_color = 8;
  const int col_y = topleft.y;

  context_.font->render_text_onto(*ctx.surface, "qual", ViewPoint{210, col_y},
                                  FontFace::SUPERSMA, text_color,
                                  AlignX::RightEdge);
  context_.font->render_text_onto(*ctx.surface, "sup", ViewPoint{248, col_y},
                                  FontFace::SUPERSMA, text_color,
                                  AlignX::RightEdge);
  context_.font->render_text_onto(*ctx.surface, "nee", ViewPoint{284, col_y},
                                  FontFace::SUPERSMA, text_color,
                                  AlignX::RightEdge);
  context_.font->render_text_onto(*ctx.surface, "use", ViewPoint{322, col_y},
                                  FontFace::SUPERSMA, text_color,
                                  AlignX::RightEdge);
  context_.font->render_text_onto(*ctx.surface, "days", ViewPoint{364, col_y},
                                  FontFace::SUPERSMA, text_color,
                                  AlignX::RightEdge);

  // Vertical separators between columns
  constexpr int line_color = 15;
  constexpr int line_width = 2;
  const ViewPoint line_top_base{168, 44};
  const ViewPoint line_bottom_base{168, 324};
  const int offsets[] = {0, 48, 88, 124, 160};
  for (int off : offsets) {
    gfx::draw::line(*ctx.surface, {line_top_base.x + off, line_top_base.y},
                    {line_bottom_base.x + off, line_bottom_base.y}, line_color,
                    line_width);
  }

  auto [start_id, end_id] = core::food::CategoryRegistry::id_range(category_);

  const int name_x = topleft.x;
  int y = topleft.y + 20;
  constexpr int line_height = 12;

  for (int ing_id = start_id; ing_id < end_id; ing_id++) {
    auto* ingredient = IngredientFactory::create_ingredient(ing_id);
    if (!ingredient) {
      break;
    }

    const auto* stock = inventory.get_ingredient_stock(ingredient->type());

    const int quality = stock ? stock->weighted_avg_quality : 0;
    const int supplies = stock ? stock->quantity / 1000 : 0;
    const int usage_grams = demand_grams[ing_id];
    const int need = (usage_grams > 0) ? (usage_grams / 1000) + 1 : 0;
    const int usage = usage_grams / 1000;
    const int days_left =
        stock ? std::min(999, stock->days_remaining(usage_grams)) : 0;

    context_.font->render_text_onto(*ctx.surface, ingredient->name(),
                                    ViewPoint{name_x, y}, FontFace::CSTANDAR,
                                    text_color);

    constexpr int dark_red = 20;
    const int row_color = need > supplies ? dark_red : int{text_color};

    if (supplies || need || usage > 0) {
      context_.font->render_text_onto(*ctx.surface, std::to_string(quality),
                                      ViewPoint{210, y}, FontFace::CSTANDAR,
                                      row_color, AlignX::RightEdge);
      context_.font->render_text_onto(*ctx.surface, std::to_string(supplies),
                                      ViewPoint{250, y}, FontFace::CSTANDAR,
                                      row_color, AlignX::RightEdge);
      context_.font->render_text_onto(*ctx.surface, std::to_string(need),
                                      ViewPoint{288, y}, FontFace::CSTANDAR,
                                      row_color, AlignX::RightEdge);
      context_.font->render_text_onto(*ctx.surface, std::to_string(usage),
                                      ViewPoint{326, y}, FontFace::CSTANDAR,
                                      row_color, AlignX::RightEdge);
    }

    if (supplies > 0 && days_left > 0) {
      context_.font->render_text_onto(*ctx.surface, std::to_string(days_left),
                                      ViewPoint{358, y}, FontFace::CSTANDAR,
                                      text_color, AlignX::RightEdge);
    }

    y += line_height;
  }
}

void IngredientCategoryView::set_up_input_panel() {
  input_panel_->delete_buttons();

  input_panel_->add_button(Icon::VEGETABLE, ButtonType::Square,
                           ButtonPosition::TopLeft, "Vegetables", [this]() {
                             category_ = IngredientCategory::VEGETABLES;
                             mark_dirty();
                           });

  input_panel_->add_button(Icon::MEAT, ButtonType::Square,
                           ButtonPosition::TopCenter, "Meat", [this]() {
                             category_ = IngredientCategory::MEAT;
                             mark_dirty();
                           });

  input_panel_->add_button(Icon::FRUIT, ButtonType::Square,
                           ButtonPosition::MiddleLeft, "Fruit", [this]() {
                             category_ = IngredientCategory::FRUIT;
                             mark_dirty();
                           });

  input_panel_->add_button(Icon::CHEESE, ButtonType::Square,
                           ButtonPosition::MiddleCenter, "Cheese", [this]() {
                             category_ = IngredientCategory::CHEESE;
                             mark_dirty();
                           });

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this]() { state_.ui().pop(); });

  input_panel_->add_button(
      Icon::CONTRACTS, ButtonType::Vertical, ButtonPosition::TopRight,
      "Contracts", [this]() {
        auto* player = state_.active_player();
        auto* restaurant = state_.active_restaurant();
        auto& contracts = player->contracts();
        const bool has_contracts =
            std::any_of(contracts.begin(), contracts.end(), [&](const auto& c) {
              return c.restaurant_id == restaurant->id();
            });
        if (has_contracts) {
          state_.ui().push<ContractsViewState>();
        }
        else {
          context_.popup->create<AlertPopup>("No contracts!");
        }
      });

  input_panel_->add_button(
      Icon::SHOPPING_CART, ButtonType::Horizontal, ButtonPosition::BottomLeft,
      "Shop", [this]() {
        state_.ui().replace_top<IngredientCategoryViewState>(category_);
        state_.ui().push<IngredientSupplierListState>(category_);
      });
}

void IngredientCategoryView::navigate_to_restaurant(core::RestaurantId new_id) {
  state_.active_restaurant(new_id);
  state_.ui().replace_top<IngredientCategoryViewState>(category_);
}
