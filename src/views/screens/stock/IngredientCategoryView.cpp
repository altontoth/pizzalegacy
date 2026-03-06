/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/stock/IngredientCategoryView.h"

#include "core/city/CentralDepot.h"
#include "core/city/City.h"
#include "core/food/IngredientCategory.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/Alignment.h"
#include "engine/ui/InteractionEvent.h"
#include "factories/IngredientFactory.h"
#include "gfx/Blit.h"
#include "gfx/Draw.h"
#include "resources/BlitmapManager.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"
#include "views/ui/components/BeveledButton.h"

IngredientCategoryView::IngredientCategoryView(GameContext& context,
                                               GameState& state,
                                               IngredientCategory category,
                                               IngredientScreenMode mode)
    : StockMain(context, state), category_(category), mode_(mode) {

  set_up_input_panel();
  status_controller_.show_restaurant(
      {.state = &state_, .rest = state_.active_restaurant(), .label = "STOCK"});
}

void IngredientCategoryView::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  ctx.surface->palette({60});
  if (!show_priorities_) {
    draw_page_header(ctx);
    draw_page_screen(ctx);
  }
  else {
    draw_priorities(ctx);
  }
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

  std::string title;
  ViewPoint title_pos;
  if (mode_ == IngredientScreenMode::CENTRAL_DEPOT) {
    title = "central depot " + std::string(state_.active_city()->name());
    title_pos = {134, 12};
  }
  else {
    title = "supplies " +
            std::string(core::food::CategoryRegistry::name(category_));
    title_pos = {138, 22};
  }

  context_.font->render_text_onto(*ctx.surface, title, title_pos,
                                  FontFace::PIZ_NUMB, 8, AlignX::Left);
}

void IngredientCategoryView::draw_page_screen(const RenderContext& ctx) {
  draw_ingredient_list(ctx);
}

void IngredientCategoryView::draw_depot_overview(const RenderContext& ctx) {
  auto& depot =
      state_.active_city()->central_depot(state_.active_player()->id().value);
  const auto& stocks = depot.all_stocks();

  // Calculate total kilograms and value
  int total_kg = 0;
  Money total_value{0};

  // Sum across all ingredients in depot
  for (const auto& stock : stocks) {
    total_kg += stock.quantity;
    total_value =
        total_value + Money{stock.weighted_avg_cost.cents * stock.quantity};
  }

  // Draw depot info box
  ViewPoint base{30, 86};
  constexpr int line_height = 20;
  constexpr int text_black = 8;

  // Label column
  context_.font->render_text_onto(*ctx.surface,
                                  "stocks depot:", ViewPoint{base.x, base.y},
                                  FontFace::PIZ_NUMB, text_black);
  context_.font->render_text_onto(
      *ctx.surface, "kilograms:", ViewPoint{base.x, base.y + line_height},
      FontFace::PIZ_NUMB, text_black);
  context_.font->render_text_onto(
      *ctx.surface, "value:", ViewPoint{base.x, base.y + line_height * 2},
      FontFace::PIZ_NUMB, text_black);

  // Value column (right-aligned)
  int value_x = base.x + 180;
  context_.font->render_text_onto(*ctx.surface, compat::format("{}", total_kg),
                                  ViewPoint{value_x, base.y + line_height},
                                  FontFace::PIZ_NUMB, text_black,
                                  AlignX::Right);

  context_.font->render_text_onto(
      *ctx.surface, compat::format("${:.2f}", total_value.cents / 100.0),
      ViewPoint{value_x, base.y + line_height * 2}, FontFace::PIZ_NUMB,
      text_black, AlignX::Right);
}

void IngredientCategoryView::draw_ingredient_list(const RenderContext& ctx) {
  auto* restaurant = state_.active_restaurant();
  const bool is_depot = mode_ == IngredientScreenMode::CENTRAL_DEPOT;

  // For depot mode, read stock from central depot; for restaurant, from
  // inventory
  core::city::CentralDepot* depot_ptr = nullptr;
  if (is_depot) {
    depot_ptr = &state_.active_city()->central_depot(
        state_.active_player()->id().value);
  }
  const auto& inventory = restaurant->inventory();

  // Calculate daily ingredient demand in grams (always from restaurant)
  core::food::IngredientDemand demand_grams{};

  auto* player = state_.active_player();
  if (is_depot) {
    demand_grams =
        state_.total_ingredient_need_for_player(state_.active_player()->id());
  }
  else {
    demand_grams =
        restaurant->calculate_daily_ingredient_demand_grams(player->recipes());
  }

  // Column headers - positioned above the data columns
  const ViewPoint topleft = is_depot ? ViewPoint{14, 30} : ViewPoint{52, 44};
  const uint8_t text_color = 8;
  const FontFace column_font =
      is_depot ? FontFace::CSTANDAR : FontFace::SUPERSMA;
  const int col_y = topleft.y;

  // Quality for both:
  const auto col1_pos =
      is_depot ? ViewPoint{176, col_y} : ViewPoint{210, col_y};
  const std::string_view col1_label = "qual";
  // Stoc(k) for depot, sup(ply) for restaurant
  const auto col2_pos =
      is_depot ? ViewPoint{226, col_y} : ViewPoint{248, col_y};
  const std::string_view col2_label = is_depot ? "stoc" : "sup";
  // Nee(d) for both - this is the value we'd need if we met 100% of demand
  const auto col3_pos =
      is_depot ? ViewPoint{266, col_y} : ViewPoint{284, col_y};
  const std::string_view col3_label = "nee";
  // value in depot, use in restaurant - use is actual usage
  const auto col4_pos =
      is_depot ? ViewPoint{326, col_y} : ViewPoint{322, col_y};
  const std::string_view col4_label = is_depot ? "value" : "use";
  // LV (last purchase value) in depot, days in restaurant
  const auto col5_pos =
      is_depot ? ViewPoint{356, col_y} : ViewPoint{364, col_y};
  const std::string_view col5_label = is_depot ? "lv" : "days";

  // Column X positions (RightEdge-aligned for numbers)
  const int col1_val_x = is_depot ? 176 : 210;
  const int col2_val_x = is_depot ? 224 : 250;
  const int col3_val_x = is_depot ? 260 : 288;
  const int col4_val_x = is_depot ? 308 : 326;
  const int col5_val_x = is_depot ? 360 : 358;

  // Draw column headers:
  context_.font->render_text_onto(*ctx.surface, col1_label, col1_pos,
                                  column_font, text_color, AlignX::RightEdge);
  context_.font->render_text_onto(*ctx.surface, col2_label, col2_pos,
                                  column_font, text_color, AlignX::RightEdge);
  context_.font->render_text_onto(*ctx.surface, col3_label, col3_pos,
                                  column_font, text_color, AlignX::RightEdge);
  context_.font->render_text_onto(*ctx.surface, col4_label, col4_pos,
                                  column_font, text_color, AlignX::RightEdge);
  context_.font->render_text_onto(*ctx.surface, col5_label, col5_pos,
                                  column_font, text_color, AlignX::RightEdge);

  // Draw vertical bars only visible in restaurant view:
  if (!is_depot) {
    ViewPoint line_top{168, 44};
    ViewPoint line_bottom{168, 324};
    const int line_color = 15;
    const int line_width = 2;
    // Ingredient name | Qual
    gfx::draw::line(*ctx.surface, line_top, line_bottom, line_color,
                    line_width);
    line_top.x += 48;
    line_bottom.x += 48;
    // Qual |  Sup
    gfx::draw::line(*ctx.surface, line_top, line_bottom, line_color,
                    line_width);
    line_top.x += 40;
    line_bottom.x += 40;
    // Sup | Nee
    gfx::draw::line(*ctx.surface, line_top, line_bottom, line_color,
                    line_width);
    line_top.x += 36;
    line_bottom.x += 36;
    // Nee | Use
    gfx::draw::line(*ctx.surface, line_top, line_bottom, line_color,
                    line_width);
    line_top.x += 36;
    line_bottom.x += 36;
    // Use | Days
    gfx::draw::line(*ctx.surface, line_top, line_bottom, line_color,
                    line_width);
  }

  // Get ingredient ID range for this category
  auto [start_id, end_id] = core::food::CategoryRegistry::id_range(category_);

  // Render ingredients
  const int name_x = topleft.x;
  int y = topleft.y + 20;
  const int line_height = 12;

  for (int ing_id = start_id; ing_id < end_id; ing_id++) {
    auto* ingredient = IngredientFactory::create_ingredient(ing_id);
    if (!ingredient) {
      break;
    }

    // Get current stock from depot or restaurant inventory
    const auto* stock =
        is_depot ? depot_ptr->get_ingredient_stock(ing_id)
                 : inventory.get_ingredient_stock(ingredient->type());
    int quality = stock ? stock->weighted_avg_quality : 0;
    int supplies = stock ? stock->quantity / 1000 : 0;

    // Get demand from calculated array (in grams)
    int usage_grams = demand_grams[ing_id];

    // NEE column: Daily demand in kilograms with +1 buffer
    int need = (usage_grams > 0) ? (usage_grams / 1000) + 1 : 0;

    // USE column: Actual usage in kilograms (rounded down)
    int usage = usage_grams / 1000;

    // Days remaining (or LV for depot) calculation
    int days_left = std::min(999, stock->days_remaining(usage_grams));
    std::string col5;
    if (is_depot) {
      col5 = compat::format("{}", stock->last_price);
    }
    else {
      col5 = std::to_string(days_left);
    }

    // Render ingredient name (left-aligned)
    context_.font->render_text_onto(*ctx.surface, ingredient->name(),
                                    ViewPoint{name_x, y}, FontFace::CSTANDAR,
                                    text_color);

    const int black = 8;
    const int dark_red = 20;

    auto this_row_color = need > supplies ? dark_red : black;
    if (is_depot || (supplies || need || usage > 0)) {
      context_.font->render_text_onto(
          *ctx.surface, std::to_string(quality), ViewPoint{col1_val_x, y},
          FontFace::CSTANDAR, this_row_color, AlignX::RightEdge);

      context_.font->render_text_onto(
          *ctx.surface, std::to_string(supplies), ViewPoint{col2_val_x, y},
          FontFace::CSTANDAR, text_color, AlignX::RightEdge);

      context_.font->render_text_onto(
          *ctx.surface, std::to_string(need), ViewPoint{col3_val_x, y},
          FontFace::CSTANDAR, text_color, AlignX::RightEdge);

      context_.font->render_text_onto(
          *ctx.surface, std::to_string(usage), ViewPoint{col4_val_x, y},
          FontFace::CSTANDAR, text_color, AlignX::RightEdge);
    }

    // Only show days if there's stock
    if (is_depot || (supplies > 0 && days_left > 0)) {
      context_.font->render_text_onto(
          *ctx.surface, col5, ViewPoint{col5_val_x, y}, FontFace::CSTANDAR,
          text_color, AlignX::RightEdge);
    }

    y += line_height;
  }

  if (is_depot) {
    int total_sum = 0; // TODO: calculate sum value
    context_.font->render_text_onto(*ctx.surface, "sum value",
                                    ViewPoint{96, 320}, FontFace::CSTANDAR,
                                    text_color, AlignX::Right);
    context_.font->render_text_onto(*ctx.surface, std::to_string(total_sum),
                                    ViewPoint{232, 320}, FontFace::CSTANDAR,
                                    text_color, AlignX::Right);
  }
}

void IngredientCategoryView::set_up_input_panel() {
  input_panel_->delete_buttons();

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

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this]() { state_.ui().pop(); });

  if (mode_ == IngredientScreenMode::CENTRAL_DEPOT) {
    input_panel_->add_button(Icon::GLOBE, ButtonType::Square,
                             ButtonPosition::TopRight, "Export", [this]() {
                               show_priorities_ = false;
                               state_.ui().push<EuroMapState>(MapMode::DEPOT,
                                                              category_);
                             });

    input_panel_->add_button(Icon::EXCLAMATION_MARK, ButtonType::Square,
                             ButtonPosition::MiddleRight, "Priorities",
                             [this]() {
                               show_priorities_ = true;
                               mark_dirty();
                             });

    input_panel_->add_button(Icon::BUY, ButtonType::Square,
                             ButtonPosition::BottomLeft, "Buy", [this]() {
                               state_.ui().push<IngredientSupplierListState>(
                                   category_,
                                   IngredientScreenMode::CENTRAL_DEPOT);
                             });

    input_panel_->add_button(Icon::SELL, ButtonType::Square,
                             ButtonPosition::BottomCenter, "Sell", [this]() {
                               state_.ui().push<IngredientSupplierListState>(
                                   category_,
                                   IngredientScreenMode::CENTRAL_DEPOT_SELL);
                             });
  }
  else {
    input_panel_->add_button(
        Icon::CONTRACTS, ButtonType::Vertical, ButtonPosition::TopRight,
        "Contracts",
        []() { Logger::info("[Stock] Contracts not yet implemented"); });

    input_panel_->add_button(
        Icon::SHOPPING_CART, ButtonType::Horizontal, ButtonPosition::BottomLeft,
        "Shop",
        [this]() { state_.ui().push<IngredientSupplierListState>(category_); });
  }
}

SupplierType IngredientCategoryView::category_to_supplier_type(
    IngredientCategory cat) const {
  switch (cat) {
  case IngredientCategory::VEGETABLES:
    return SupplierType::VEGETABLES;
  case IngredientCategory::MEAT:
    return SupplierType::MEAT;
  case IngredientCategory::FRUIT:
    return SupplierType::FRUIT;
  case IngredientCategory::CHEESE:
    return SupplierType::CHEESE;
  default:
    return SupplierType::VEGETABLES;
  }
}

void IngredientCategoryView::draw_priorities(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back9"));

  // Header
  const int text_color = 8;
  const ViewPoint header_pos{116, 18};
  context_.font->render_line_onto(*ctx.surface, "delivery priorities",
                                  header_pos, FontFace::PIZ_NUMB, text_color);

  // Sync priority list with current restaurants
  auto& depot =
      state_.active_city()->central_depot(state_.active_player()->id().value);
  auto player_restaurants =
      state_.restaurants_for(state_.active_player()->id());

  std::vector<core::RestaurantId> restaurant_ids;
  for (auto* r : player_restaurants) {
    restaurant_ids.push_back(r->id());
  }
  depot.sync_delivery_priorities(restaurant_ids);
  const auto& priorities = depot.delivery_priorities();

  // Row config
  const int row_height = 24;
  ViewRect start_box{76, 42, 240, 16};
  ViewPoint label_pos{46, 46};
  int slot_count = std::min(static_cast<int>(priorities.size()),
                            core::city::max_delivery_priority_slots);

  for (int row_nr = 0; row_nr < slot_count; row_nr++) {
    ViewRect this_box{start_box.x, start_box.y + (row_height * row_nr),
                      start_box.w, start_box.h};
    ViewPoint this_label = label_pos + PxOffset{0, row_height * row_nr};

    context_.font->render_line_onto(*ctx.surface, std::to_string(row_nr + 1),
                                    this_label, FontFace::CSTANDAR, text_color);

    // Look up restaurant name
    auto* restaurant = state_.restaurant(priorities[row_nr]);
    std::string name = restaurant ? restaurant->name() : "???";

    int captured_row = row_nr;
    core::RestaurantId captured_id = priorities[row_nr];
    interactions_.create_frame_item(
        this_box, "row_" + std::to_string(row_nr),
        [this, captured_row, captured_id](views::ui::InteractionEvent ie) {
          if (ie.type == views::ui::InteractionEvent::Type::ButtonDown) {
            pressed_priority_row_ = captured_row;
            mark_dirty();
          }
          else if (ie.type == views::ui::InteractionEvent::Type::Click) {
            auto& d = state_.active_city()->central_depot(
                state_.active_player()->id().value);
            d.promote_to_top(captured_id);
            pressed_priority_row_ = -1;
            mark_dirty();
          }
          else if (ie.type == views::ui::InteractionEvent::Type::Cancelled) {
            pressed_priority_row_ = -1;
            mark_dirty();
          }
        });

    bool is_pressed = (pressed_priority_row_ == row_nr);
    views::ui::components::draw_beveled_button(
        context_.font, context_.vga->get("gfx/permap"), *ctx.surface, this_box,
        name,
        is_pressed ? views::ui::components::ButtonState::Pressed
                   : views::ui::components::ButtonState::Normal);
  }
}
