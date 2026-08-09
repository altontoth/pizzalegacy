/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/restaurant/WeeksTakings.h"

#include "core/city/Property.h"
#include "core/restaurant/PizzaSalesAggregator.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/util/LegacyConstants.h"
#include "engine/Logger.h"
#include "gfx/Draw.h"
#include "resources/BlitmapManager.h"
#include "resources/FontManager.h"
#include "views/base/ViewDefs.h"
#include "views/ui/PageDecoration.h"
#include "views/ui/finance/TurnInvoicePopups.h"

constexpr int header_y = 22;
constexpr int header_text_color = 240;
constexpr int header_secondary_color = 11;

WeeksTakings::WeeksTakings(GameContext& context, GameState& state,
                           core::RestaurantId restaurant_id,
                           bool end_of_week_mode, std::function<void()> on_done)
    : RestaurantSubView(context, state, restaurant_id),
      end_of_week_mode_(end_of_week_mode), on_done_(std::move(on_done)) {
  restaurant_ = state_.restaurant(restaurant_id_);
  create_buttons();
}

void WeeksTakings::tick() {}

void WeeksTakings::render(const RenderContext& ctx) {
  ctx.surface->palette(PaletteHandle{8});
  ctx.surface->fill(29);

  render_background(ctx);

  auto weekly = aggregate_weekly_takings();

  if (current_page_ <= 1) {
    // Pages 0-1: Pizza sales
    render_pizza_sales_page(ctx, weekly, current_page_);
  }
  else {
    // Page 2: Expenses
    render_expenses_page(ctx, weekly);
  }
}

void WeeksTakings::create_buttons() {
  using BT = ButtonType;

  // Next page button
  input_panel_->add_button(Icon::ARROW_UP, BT::Horizontal,
                           ButtonPosition::TopLeft, "Next Page", [this]() {
                             if (current_page_ < 2) {
                               current_page_++;
                               mark_dirty();
                             }
                           });

  // Previous page button
  input_panel_->add_button(Icon::ARROW_DOWN, BT::Horizontal,
                           ButtonPosition::MiddleLeft, "Previous Page",
                           [this]() {
                             if (current_page_ > 0) {
                               current_page_--;
                               mark_dirty();
                             }
                           });

  // Back button - shows staff invoice popup then pops
  input_panel_->add_button(Icon::ARROW_BACK, BT::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this]() { dismiss(); });
}

void WeeksTakings::render_background(const RenderContext& ctx) {
  const auto& stanbobs = context_.bob->get("bob/stanbobs.bob");
  const int edge_offset = 6;
  ViewRect box{0, 0, views::base::MAIN_PANEL_WIDTH,
               views::base::MAIN_PANEL_HEIGHT};
  views::ui::render_corner_decorations(
      *ctx.surface, context_.blitmap->get<Blitmap2Icon>(), box);
  views::ui::render_decorated_corners(
      *ctx.surface, stanbobs,
      ViewRect{{edge_offset, edge_offset},
               {views::base::MAIN_PANEL_WIDTH - (edge_offset * 2),
                views::base::MAIN_PANEL_HEIGHT - (edge_offset * 2)}},
      edge_offset, {11});
}

WeeksTakings::WeeklyTakings WeeksTakings::aggregate_weekly_takings() const {
  WeeklyTakings result;

  auto week_data =
      restaurant_->stats().current_week(state_.time().current_date());

  for (const auto& day : week_data) {
    // Sum ingredient purchases by category (for expenses report)
    for (int i = 0; i < 4; ++i) {
      if (day.ingredient_purchases_by_category[i].cents > 0) {
        Logger::debug("[WeeksTakings] Day has purchase cat[{}] = {}", i,
                      day.ingredient_purchases_by_category[i]);
      }
      result.ingredient_purchases_by_category[i] =
          result.ingredient_purchases_by_category[i] +
          day.ingredient_purchases_by_category[i];
    }
  }

  Logger::debug("[WeeksTakings] Week purchase totals: V={}, M={}, F={}, C={}",
                result.ingredient_purchases_by_category[0],
                result.ingredient_purchases_by_category[1],
                result.ingredient_purchases_by_category[2],
                result.ingredient_purchases_by_category[3]);

  if (restaurant_) {
    result.pizza_sales = core::restaurant::aggregate_weekly_pizza_sales(
        *restaurant_, state_.time().current_date());
    for (const auto& [recipe_id, sales] : result.pizza_sales) {
      result.total_revenue = result.total_revenue + sales.revenue;
      result.total_costs = result.total_costs + sales.ingredient_cost;
      result.total_profit = result.total_profit + sales.profit;
    }
  }

  // Calculate weekly staff wages
  if (restaurant_) {
    for (const auto& cook : restaurant_->cooks()) {
      if (cook) {
        result.total_staff_wages += cook->payment();
      }
    }
    for (const auto& waiter : restaurant_->waiters()) {
      if (waiter) {
        result.total_staff_wages += waiter->payment();
      }
    }
    if (restaurant_->manager()) {
      result.total_staff_wages += restaurant_->manager()->payment();
    }
  }

  return result;
}

void WeeksTakings::render_pizza_sales_page(const RenderContext& ctx,
                                           const WeeklyTakings& weekly,
                                           int page) {
  const int PIZZAS_PER_PAGE = 12;
  const int start_idx = page * PIZZAS_PER_PAGE;
  const int end_idx = std::min(start_idx + PIZZAS_PER_PAGE,
                               static_cast<int>(weekly.pizza_sales.size()));

  const int text_color = 240;
  context_.font->render_text_onto(*ctx.surface, "week's",
                                  ViewPoint{72, header_y}, FontFace::ASTANDAR,
                                  header_text_color, TextAnchor::Start);
  TextStyle ts{.color = header_text_color,
               .align = TextAnchor::Start,
               .shadow = ShadowStyle{
                   .offset = {-2, 2},
                   .color = header_secondary_color,
               }};
  context_.font->render_text_onto(*ctx.surface, "takings", ViewPoint{178, 24},
                                  FontFace::ASTANDAR, ts);

  // Subtitle
  context_.font->render_text_onto(*ctx.surface, "sales", ViewPoint{160, 44},
                                  FontFace::ASTANDAR, 11, TextAnchor::Start);

  // Column headers
  context_.font->render_text_onto(*ctx.surface, "ITEMS SALES COSTS PROFIT",
                                  ViewPoint{168, 64}, FontFace::SUPERSMA,
                                  text_color, TextAnchor::Start);

  // Column x positions
  const int name_x = 42 - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  const int items_x = 222 - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  const int sales_x = 268 - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  const int costs_x = 320 - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  const int profit_x = 380 - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;

  int y = 106 - LegacyConstants::WOOD_BORDER_HEIGHT_TOP;
  const int line_spacing = 14;

  Money page_revenue{0};
  Money page_costs{0};
  Money page_profit{0};
  int page_items = 0;

  // Render each pizza on this page
  for (int i = start_idx; i < end_idx; ++i) {
    const auto& [recipe_id, sales] = weekly.pizza_sales[i];

    // Get pizza name from menu
    std::string pizza_name = "Unknown";
    if (restaurant_) {
      auto owner_id = restaurant_->property().owner_or_tenant();
      auto* owner =
          owner_id ? state_.player(*owner_id) : state_.active_player();
      const auto* recipe = owner ? owner->recipes().find(recipe_id) : nullptr;
      if (recipe) {
        pizza_name = std::string(recipe->display_name());
      }
    }

    // Render pizza name
    context_.font->render_text_onto(*ctx.surface, pizza_name,
                                    ViewPoint{name_x, y}, FontFace::SUPERSMA,
                                    text_color, TextAnchor::Start);

    // Items sold
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{}", sales.count_sold),
        ViewPoint{items_x, y}, FontFace::SUPERSMA, text_color, TextAnchor::End);

    // Sales (revenue)
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{}", sales.revenue),
        ViewPoint{sales_x, y}, FontFace::SUPERSMA, text_color, TextAnchor::End);

    // Costs
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{}", sales.ingredient_cost),
        ViewPoint{costs_x, y}, FontFace::SUPERSMA, text_color, TextAnchor::End);

    // Profit
    context_.font->render_text_onto(*ctx.surface,
                                    compat::format("{}", sales.profit),
                                    ViewPoint{profit_x, y}, FontFace::SUPERSMA,
                                    text_color, TextAnchor::End);

    page_revenue = page_revenue + sales.revenue;
    page_costs = page_costs + sales.ingredient_cost;
    page_profit = page_profit + sales.profit;
    page_items += sales.count_sold;

    y += line_spacing;
  }

  // Total
  const int total_y = 308;
  context_.font->render_text_onto(*ctx.surface,
                                  "TOTAL:", ViewPoint{20, total_y},
                                  FontFace::SUPERSMA, 11, TextAnchor::Start);

  context_.font->render_text_onto(
      *ctx.surface, compat::format("{}", page_items),
      ViewPoint{items_x, total_y}, FontFace::SUPERSMA, 11, TextAnchor::End);

  context_.font->render_text_onto(
      *ctx.surface, compat::format("{}", page_revenue),
      ViewPoint{sales_x, total_y}, FontFace::SUPERSMA, 11, TextAnchor::End);

  context_.font->render_text_onto(
      *ctx.surface, compat::format("{}", page_costs),
      ViewPoint{costs_x, total_y}, FontFace::SUPERSMA, 11, TextAnchor::End);

  context_.font->render_text_onto(
      *ctx.surface, compat::format("{}", page_profit),
      ViewPoint{profit_x, total_y}, FontFace::SUPERSMA, 11, TextAnchor::End);
}

void WeeksTakings::render_expenses_page(const RenderContext& ctx,
                                        const WeeklyTakings& weekly) {

  context_.font->render_text_onto(*ctx.surface, "week's",
                                  ViewPoint{72, header_y}, FontFace::ASTANDAR,
                                  header_text_color, TextAnchor::Start);
  TextStyle ts{.color = header_text_color,
               .align = TextAnchor::Start,
               .shadow = ShadowStyle{
                   .offset = {-2, 2},
                   .color = header_secondary_color,
               }};
  context_.font->render_text_onto(*ctx.surface, "expenses", ViewPoint{178, 20},
                                  FontFace::ASTANDAR, ts);

  const int text_color = 10;
  const std::string no_name = "-----------";

  const int category_x = 18;
  const int label_x = 100;
  const int value_x = 310;
  const int total_x = 376;
  int y = 48;
  const int line_spacing = 14;

  gfx::draw::line(*ctx.surface, {16, 108}, {358, 108}, 14, 2);
  gfx::draw::line(*ctx.surface, {96, 44}, {96, 330}, 14, 2);
  gfx::draw::line(*ctx.surface, {256, 44}, {256, 330}, 14, 2);

  // Ingredients section header
  context_.font->render_text_onto(*ctx.surface, "ingre-",
                                  ViewPoint{category_x, y}, FontFace::SUPERSMA,
                                  text_color, TextAnchor::Start);
  context_.font->render_text_onto(
      *ctx.surface, "dients:", ViewPoint{category_x, y + 12},
      FontFace::SUPERSMA, text_color, TextAnchor::Start);

  const std::array<std::string, 4> category_names = {
      "Vegetables", "Meat + Fish", "Fruit", "Cheese + Spices"};

  Money total_ingredients{0};

  // Render ingredient categories
  for (size_t i = 0; i < category_names.size(); ++i) {
    context_.font->render_text_onto(*ctx.surface, category_names[i],
                                    ViewPoint{label_x, y}, FontFace::SUPERSMA,
                                    text_color, TextAnchor::Start);

    context_.font->render_text_onto(
        *ctx.surface,
        compat::format("{}", weekly.ingredient_purchases_by_category[i]),
        ViewPoint{value_x, y}, FontFace::SUPERSMA, text_color, TextAnchor::End);

    total_ingredients =
        total_ingredients + weekly.ingredient_purchases_by_category[i];
    y += line_spacing;
  }

  // total goes on the previous line
  y -= line_spacing;

  // Ingredient subtotal
  context_.font->render_text_onto(
      *ctx.surface, compat::format("{}", total_ingredients),
      ViewPoint{total_x, y}, FontFace::SUPERSMA, text_color, TextAnchor::End);

  y += 22;

  // Cooks section
  context_.font->render_text_onto(
      *ctx.surface, "cooks:", ViewPoint{category_x, y}, FontFace::SUPERSMA,
      text_color, TextAnchor::Start);

  Money total_staff{0};

  // Render 6 cook slots
  if (restaurant_) {
    for (int i = 0; i < MAXIMUM_STAFF_PER_CATEGORY; ++i) {
      const auto& cook_slot = restaurant_->cooks()[i];
      if (cook_slot) {
        std::string name = cook_slot->character().first_name() + " " +
                           cook_slot->character().last_name();
        Money weekly_wage = cook_slot->payment();

        context_.font->render_text_onto(
            *ctx.surface, name, ViewPoint{label_x, y}, FontFace::SUPERSMA,
            text_color, TextAnchor::Start);
        context_.font->render_text_onto(
            *ctx.surface, compat::format("{}", weekly_wage),
            ViewPoint{value_x, y}, FontFace::SUPERSMA, text_color,
            TextAnchor::End);
        total_staff = total_staff + weekly_wage;
      }
      else {
        context_.font->render_text_onto(
            *ctx.surface, no_name, ViewPoint{label_x, y}, FontFace::SUPERSMA,
            text_color, TextAnchor::Start);
      }
      y += line_spacing;
    }
  }

  y += 4;

  // Waiters section
  context_.font->render_text_onto(*ctx.surface, "waiters",
                                  ViewPoint{category_x, y}, FontFace::SUPERSMA,
                                  text_color, TextAnchor::Start);

  // Render 6 waiter slots
  if (restaurant_) {
    for (int i = 0; i < MAXIMUM_STAFF_PER_CATEGORY; ++i) {
      const auto& waiter_slot = restaurant_->waiters()[i];
      if (waiter_slot) {
        std::string name = waiter_slot->character().first_name() + " " +
                           waiter_slot->character().last_name();
        Money weekly_wage = waiter_slot->payment();

        context_.font->render_text_onto(
            *ctx.surface, name, ViewPoint{label_x, y}, FontFace::SUPERSMA,
            text_color, TextAnchor::Start);
        context_.font->render_text_onto(
            *ctx.surface, compat::format("{}", weekly_wage),
            ViewPoint{value_x, y}, FontFace::SUPERSMA, text_color,
            TextAnchor::End);
        total_staff = total_staff + weekly_wage;
      }
      else {
        context_.font->render_text_onto(
            *ctx.surface, no_name, ViewPoint{label_x, y}, FontFace::SUPERSMA,
            text_color, TextAnchor::Start);
      }
      y += line_spacing;
    }
  }

  y += 3;

  // Manager section
  context_.font->render_text_onto(
      *ctx.surface, "managers:", ViewPoint{category_x, y}, FontFace::SUPERSMA,
      text_color, TextAnchor::Start);

  if (restaurant_ && restaurant_->manager()) {
    std::string name = restaurant_->manager()->character().first_name() + " " +
                       restaurant_->manager()->character().last_name();
    Money weekly_wage = restaurant_->manager()->payment();

    context_.font->render_text_onto(*ctx.surface, name, ViewPoint{label_x, y},
                                    FontFace::SUPERSMA, text_color,
                                    TextAnchor::Start);
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{}", weekly_wage), ViewPoint{value_x, y},
        FontFace::SUPERSMA, text_color, TextAnchor::End);
    total_staff = total_staff + weekly_wage;
  }
  else {
    context_.font->render_text_onto(*ctx.surface, no_name,
                                    ViewPoint{label_x, y}, FontFace::SUPERSMA,
                                    text_color, TextAnchor::Start);
  }

  // Staff subtotal
  context_.font->render_text_onto(
      *ctx.surface, compat::format("{}", total_staff), ViewPoint{total_x, y},
      FontFace::SUPERSMA, text_color, TextAnchor::End);

  // Grand total at bottom
  const int total_y = 312;
  context_.font->render_text_onto(
      *ctx.surface, "TOTAL:", ViewPoint{category_x + 8, total_y},
      FontFace::SUPERSMA, text_color, TextAnchor::Start);

  Money grand_total = total_ingredients + total_staff;
  context_.font->render_text_onto(
      *ctx.surface, compat::format("{}", grand_total), ViewPoint{366, total_y},
      FontFace::SUPERSMA, text_color, TextAnchor::End);
}

void WeeksTakings::navigate_to_restaurant(core::RestaurantId new_id) {
  if (end_of_week_mode_) {
    return;
  }
  state_.active_restaurant(new_id);
  state_.ui().replace_top<WeeksTakingsState>(WeeksTakingsState{.id = new_id});
}

void WeeksTakings::dismiss() {
  if (end_of_week_mode_) {
    views::ui::finance::show_staff_invoice_popup(context_, state_,
                                                 restaurant_id_, on_done_);
  }
  state_.ui().pop();
}
