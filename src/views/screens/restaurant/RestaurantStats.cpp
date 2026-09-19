/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/restaurant/RestaurantStats.h"
#include "core/Demographics.h"
#include "core/city/Property.h"
#include "core/finance/Ledger.h"
#include "core/finance/Scale.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/util/LegacyConstants.h"
#include "engine/gfx/Draw.h"
#include "resources/BlitmapManager.h"
#include "resources/FontManager.h"
#include "views/base/ViewDefs.h"
#include "views/ui/PageDecoration.h"

#include <algorithm>

RestaurantStats::RestaurantStats(GameContext& context, GameState& state,
                                 core::RestaurantId restaurant_id,
                                 std::optional<RestaurantStatsScreen> screen,
                                 bool automatic_mode)
    : RestaurantSubView(context, state, restaurant_id),
      current_screen_(screen.value_or(RestaurantStatsScreen::Daily_Balance)),
      automatic_mode_(automatic_mode),
      stanbobs_(context_.bob->get("bob/stanbobs.bob")),
      blitmap2_(context_.blitmap->get<Blitmap2Icon>()) {

  restaurant_ = state_.restaurant(restaurant_id_);
  create_buttons();
}

void RestaurantStats::tick() {}

void RestaurantStats::render(const RenderContext& ctx) {
  ctx.surface->palette(PaletteHandle{8});
  uint8_t bg_color = 21;
  ctx.surface->fill(bg_color);

  switch (current_screen_) {
  case RestaurantStatsScreen::Daily_Balance:
    render_daily_balance(ctx);
    break;
  case RestaurantStatsScreen::Week_Review:
    render_week_review(ctx);
    break;
  case RestaurantStatsScreen::Month_Review:
    render_month_review(ctx);
    break;
  case RestaurantStatsScreen::Customer_Stats:
    render_customer_stats(ctx);
    break;
  case RestaurantStatsScreen::Sales_Curve:
    render_sales_curve(ctx);
    break;
  case RestaurantStatsScreen::Weeks_Takings:
  case RestaurantStatsScreen::None:
    // Handled by WeeksTakings class
    break;
  }
}

void RestaurantStats::create_buttons() {
  using RS = RestaurantStatsScreen;
  using BT = ButtonType;

  // In automatic mode (end of day report), only show the dismiss button
  if (automatic_mode_) {
    input_panel_->add_button(Icon::SELL, BT::Horizontal,
                             ButtonPosition::BottomCenter, "", [this]() {
                               state_.ui().pop();
                               state_.turn().advance_pipeline();
                             });
    return;
  }

  auto make_screen_button = [this](BT type, RS screen, Icon icon,
                                   ButtonPosition pos,
                                   const std::string& tooltip) {
    input_panel_->add_button(icon, type, pos, tooltip, [this, screen]() {
      if (screen == RS::None) {
        state_.ui().pop();
      }
      else {
        current_screen_ = screen;
        mark_dirty();
        Logger::debug("Switched to restaurant stats screen: {}",
                      static_cast<int>(screen));
      }
    });
  };

  make_screen_button(BT::Horizontal, RS::Daily_Balance, Icon::DAILY_BALANCE,
                     ButtonPosition::TopLeft, "Daily Balance");
  make_screen_button(BT::Square, RS::Week_Review, Icon::EXAMINE,
                     ButtonPosition::TopRight, "Week Review");
  make_screen_button(BT::Square, RS::Month_Review, Icon::CALCULATOR,
                     ButtonPosition::MiddleLeft, "Month Review");

  // Week's Takings button - replace current screen with same screen + current
  // state, then push WeeksTakings view
  input_panel_->add_button(
      Icon::MONEY_BAG, BT::Square, ButtonPosition::MiddleCenter,
      "Week's Takings", [this]() {
        // Replace top with current screen state so we return here on pop
        state_.ui().replace_top<RestaurantStatsState>(
            restaurant_id_, static_cast<int>(current_screen_));
        state_.ui().push<WeeksTakingsState>(restaurant_id_);
      });

  make_screen_button(BT::Square, RS::Customer_Stats, Icon::CHARACTER2,
                     ButtonPosition::MiddleRight, "Customer Stats");
  make_screen_button(BT::Horizontal, RS::Sales_Curve, Icon::BAR_CHART,
                     ButtonPosition::BottomLeft, "Sales Curve");
  make_screen_button(BT::Square, RS::None, Icon::ARROW_BACK,
                     ButtonPosition::BottomRight, "Back");
}

void RestaurantStats::render_binder(const RenderContext& ctx) {
  auto binder_start = blitmap2_.icon_surface(Blitmap2Icon::BINDER_TOP_BEGIN);
  auto binder_middle = blitmap2_.icon_surface(Blitmap2Icon::BINDER_TOP_MIDDLE);
  auto binder_end = blitmap2_.icon_surface(Blitmap2Icon::BINDER_TOP_END);

  ViewPoint binder_pos{0, 0};

  // The original game hardcoded the offsets of the binders,
  // with this modifier they are a bit tighter than if we used
  // the actual sprite widths, and it matches the original:
  constexpr int binder_width_offset = 4;

  // Draw the start piece
  ctx.surface->blit_raw(binder_start, binder_pos);
  binder_pos.x += binder_start.width() - binder_width_offset;

  // Calculate where the end piece should go
  int end_x = ctx.surface->width() - binder_end.width();

  // Fill the middle section between start and end
  while (binder_pos.x < end_x) {
    if (binder_pos.x + binder_middle.width() <= end_x) {
      ctx.surface->blit_raw(binder_middle, binder_pos);
      binder_pos.x += binder_middle.width() - binder_width_offset;
    }
    else {
      // Partial middle piece needed - clip it
      int remaining_width = end_x - binder_pos.x;
      ctx.surface->blit(binder_middle,
                        ViewRect{0, 0, remaining_width, binder_middle.height()},
                        binder_pos);
      break;
    }
  }

  // Draw the end piece at the right edge
  ctx.surface->blit_raw(binder_end, ViewPoint{end_x, 0});
}

void RestaurantStats::render_background(const RenderContext& ctx,
                                        bool show_grid) {
  ctx.surface->fill(29);
  if (show_grid) {
    constexpr uint8_t GRID_COLOR = 27;
    gfx::draw::grid(*ctx.surface, ctx.surface->rect(), GRID_COLOR);
  }

  blitmap2_.render_icon_aligned(Blitmap2Icon::PAGE_NW, *ctx.surface,
                                HAlign::Left, VAlign::Top);
  blitmap2_.render_icon_aligned(Blitmap2Icon::PAGE_NE, *ctx.surface,
                                HAlign::Right, VAlign::Top);
  blitmap2_.render_icon_aligned(Blitmap2Icon::PAGE_LEFT_BOTTOM, *ctx.surface,
                                HAlign::Left, VAlign::Bottom);
  blitmap2_.render_icon_aligned(Blitmap2Icon::PAGE_RIGHT_BOTTOM, *ctx.surface,
                                HAlign::Right, VAlign::Bottom);
  render_binder(ctx);
}

void RestaurantStats::render_daily_balance(const RenderContext& ctx) {
  views::ui::render_cork_background(*ctx.surface,
                                    context_.vga->get("gfx/permap"));
  render_paper(ctx, ViewRect{10, 14, 360, 328});
  // First draw the lines to delinitate the header area and the columns:
  gfx::draw::line(*ctx.surface, {36, 104}, {346, 104}, 14, 2);
  // Line between header and yesterday stats
  gfx::draw::line(*ctx.surface, {194, 102}, {194, 326}, 14, 2);
  // Line between yesterday and day before stats
  gfx::draw::line(*ctx.surface, {274, 102}, {274, 326}, 14, 2);

  // Header
  std::string title = restaurant_->name();
  const int center_x = ctx.surface->width() / 2;
  context_.font->render_line_onto(*ctx.surface, title, ViewPoint{center_x, 24},
                                  FontFace::PIZ_NUMB, 11, TextAnchor::Middle);
  context_.font->render_line_onto(*ctx.surface, "daily settlement",
                                  ViewPoint{center_x, 50}, FontFace::RUBY, 11,
                                  TextAnchor::Middle);

  // Column headers - automatic mode (end of day) shows "today"/"yesterday",
  // manual mode (via button) shows "yesterday"/"day before"
  const char* first_col_label = automatic_mode_ ? "today" : "yesterday";
  const char* second_col_label = automatic_mode_ ? "yesterday" : "day before";
  context_.font->render_line_onto(*ctx.surface, first_col_label,
                                  ViewPoint{172, 84}, FontFace::PIZ_NUMB, 240,
                                  TextAnchor::Start);
  context_.font->render_line_onto(*ctx.surface, second_col_label,
                                  ViewPoint{268, 84}, FontFace::PIZ_NUMB, 14,
                                  TextAnchor::Start);

  // Get stats data
  const auto& yesterday = restaurant_->stats().yesterday;
  const auto& day_before = restaurant_->stats().day_before_yesterday;

  int y = 112;
  const int label_x = 36;
  const int int_yesterday_x = 252;
  const int int_day_before_x = 332;
  const int pct_yesterday_x = 268;
  const int pct_day_before_x = 344;
  const int money_yesterday_x = 270;
  const int money_day_before_x = 356;
  const int line_height = 26;
  const int label_color = 240;
  const int yesterday_color = 240;
  const int day_before_color = 14;

  // Orders
  context_.font->render_line_onto(*ctx.surface, "orders", ViewPoint{label_x, y},
                                  FontFace::PIZ_NUMB, label_color,
                                  TextAnchor::Start);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", yesterday.orders_generated),
      ViewPoint{int_yesterday_x, y}, FontFace::PIZ_NUMB, yesterday_color,
      TextAnchor::End);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", day_before.orders_generated),
      ViewPoint{int_day_before_x, y}, FontFace::PIZ_NUMB, day_before_color,
      TextAnchor::End);
  y += line_height;

  // Pizzas sold
  context_.font->render_line_onto(*ctx.surface, "pizzas sold",
                                  ViewPoint{label_x, y}, FontFace::PIZ_NUMB,
                                  label_color, TextAnchor::Start);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", yesterday.orders_fulfilled),
      ViewPoint{int_yesterday_x, y}, FontFace::PIZ_NUMB, yesterday_color,
      TextAnchor::End);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", day_before.orders_fulfilled),
      ViewPoint{int_day_before_x, y}, FontFace::PIZ_NUMB, day_before_color,
      TextAnchor::End);
  y += line_height;

  // Delivery capacity
  context_.font->render_line_onto(*ctx.surface, "delivery capacity",
                                  ViewPoint{label_x, y}, FontFace::PIZ_NUMB,
                                  label_color, TextAnchor::Start);
  context_.font->render_line_onto(
      *ctx.surface,
      compat::format("{} %", yesterday.delivery_capacity_percent()),
      ViewPoint{pct_yesterday_x, y}, FontFace::PIZ_NUMB, yesterday_color,
      TextAnchor::End);
  context_.font->render_line_onto(
      *ctx.surface,
      compat::format("{} %", day_before.delivery_capacity_percent()),
      ViewPoint{pct_day_before_x, y}, FontFace::PIZ_NUMB, day_before_color,
      TextAnchor::End);
  y += line_height;

  // Demand value
  context_.font->render_line_onto(*ctx.surface, "demand value",
                                  ViewPoint{label_x, y}, FontFace::PIZ_NUMB,
                                  label_color, TextAnchor::Start);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", yesterday.total_order_value),
      ViewPoint{money_yesterday_x, y}, FontFace::PIZ_NUMB, yesterday_color,
      TextAnchor::End);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", day_before.total_order_value),
      ViewPoint{money_day_before_x, y}, FontFace::PIZ_NUMB, day_before_color,
      TextAnchor::End);
  y += line_height;

  // Sales
  context_.font->render_line_onto(*ctx.surface, "sales", ViewPoint{label_x, y},
                                  FontFace::PIZ_NUMB, label_color,
                                  TextAnchor::Start);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", yesterday.revenue),
      ViewPoint{money_yesterday_x, y}, FontFace::PIZ_NUMB, yesterday_color,
      TextAnchor::End);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", day_before.revenue),
      ViewPoint{money_day_before_x, y}, FontFace::PIZ_NUMB, day_before_color,
      TextAnchor::End);
  y += line_height;

  // Ingredients used
  context_.font->render_line_onto(*ctx.surface, "ingredients used",
                                  ViewPoint{label_x, y}, FontFace::PIZ_NUMB,
                                  label_color, TextAnchor::Start);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", yesterday.ingredient_costs),
      ViewPoint{money_yesterday_x, y}, FontFace::PIZ_NUMB, yesterday_color,
      TextAnchor::End);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", day_before.ingredient_costs),
      ViewPoint{money_day_before_x, y}, FontFace::PIZ_NUMB, day_before_color,
      TextAnchor::End);
  y += line_height;

  // Profit
  context_.font->render_line_onto(*ctx.surface, "profit", ViewPoint{label_x, y},
                                  FontFace::PIZ_NUMB, label_color,
                                  TextAnchor::Start);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", yesterday.gross_profit),
      ViewPoint{money_yesterday_x, y}, FontFace::PIZ_NUMB, yesterday_color,
      TextAnchor::End);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", day_before.gross_profit),
      ViewPoint{money_day_before_x, y}, FontFace::PIZ_NUMB, day_before_color,
      TextAnchor::End);
  y += line_height;

  // Expenditure (fixed costs + ingredient purchases + furniture purchases)
  context_.font->render_line_onto(*ctx.surface, "expenditure",
                                  ViewPoint{label_x, y}, FontFace::PIZ_NUMB,
                                  label_color, TextAnchor::Start);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", yesterday.total_expenditure()),
      ViewPoint{money_yesterday_x, y}, FontFace::PIZ_NUMB, yesterday_color,
      TextAnchor::End);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", day_before.total_expenditure()),
      ViewPoint{money_day_before_x, y}, FontFace::PIZ_NUMB, day_before_color,
      TextAnchor::End);
}

void RestaurantStats::render_paper(const RenderContext& ctx, ViewRect box) {
  uint8_t bg_color = 21;
  ctx.surface->fill_rect(box, bg_color);
  views::ui::render_corner_decorations(
      *ctx.surface, context_.blitmap->get<Blitmap2Icon>(), box);
  views::ui::render_push_pin(*ctx.surface, stanbobs_,
                             ViewPoint{box.x + 15, box.y + 15}, false);
  views::ui::render_push_pin(*ctx.surface, stanbobs_,
                             ViewPoint{box.x + box.w - 40, box.y + 15}, true);
}

void RestaurantStats::render_week_review(const RenderContext& ctx) {
  ctx.surface->palette({8});
  ctx.surface->fill(15);
  const int dark_text = 10;

  const int edge_offset = 6;
  views::ui::render_decorated_corners(
      *ctx.surface, stanbobs_,
      ViewRect{{edge_offset, edge_offset},
               {views::base::MAIN_PANEL_WIDTH - (edge_offset * 2),
                views::base::MAIN_PANEL_HEIGHT - (edge_offset * 2)}},
      4, {11});

  // Header
  context_.font->render_line_onto(*ctx.surface, "weekly overview",
                                  ViewPoint{192, 14}, FontFace::RUBY, dark_text,
                                  TextAnchor::Middle);

  // Column headers
  context_.font->render_line_onto(
      *ctx.surface, "guests pizzas sales    profit    %", ViewPoint{50, 102},
      FontFace::PIZ_NUMB, 11, TextAnchor::Start);

  auto week_data = restaurant_->stats().last_week();
  auto current_date = state_.time().current_date();

  // Pad or truncate to exactly 7 days
  while (week_data.size() < 7) {
    week_data.insert(week_data.begin(), core::restaurant::DailyStats{});
  }
  if (week_data.size() > 7) {
    week_data.erase(week_data.begin(),
                    week_data.begin() + (week_data.size() - 7));
  }

  const int x_pos = 14;
  const int y_start = 118;
  const int x_end = 366;

  // Draw horizontal separators between rows
  for (int i = 0; i < 8; i++) {
    int line_y = y_start + (i * 20);
    const int line_width = 2;
    const int line_color = 13;
    gfx::draw::line(*ctx.surface, {x_pos + 10, line_y}, {x_end, line_y},
                    line_color, line_width);
  }

  // Vertical separators
  gfx::draw::line(*ctx.surface, {x_pos + 10, 280}, {x_end, 280}, 13, 2);
  gfx::draw::line(*ctx.surface, {x_pos + 10, 288}, {x_end, 288}, 13, 2);

  // Draw vertical column separators
  {
    const int line_start_y = 96;
    const int line_end_y = 280;
    const std::array<int, 5> col_x_lines = {64, 112, 160, 258, 336};
    const std::array<int, 2> col_x_without_bump = {64, 160};
    for (int x : col_x_lines) {
      int y = line_start_y;
      // Two lines don't go all the way to the top
      if (std::ranges::find(col_x_without_bump, x) !=
          col_x_without_bump.end()) {
        y += 20;
      }

      gfx::draw::line(*ctx.surface, {x, y}, {x, line_end_y}, 13, 2);
    }
  }

  // Data rows
  int row_y = y_start + 4;
  int total_guests = 0;
  int total_pizzas = 0;
  Money total_sales{0};
  Money total_costs{0};
  int total_percent = 0;

  const int date_x = 36;
  const int guests_x = 108;
  const int pizzas_x = 156;
  const int sales_x = 256;
  const int profit_x = 334;
  const int percentage_x = 368;

  for (int day = 0; day < 7; day++) {
    const auto& stats = week_data[day];

    // Date column
    auto date = current_date.add_days(-(7 - day));
    std::string date_str = compat::format("{}-{}", date.day(), date.month());
    context_.font->render_line_onto(
        *ctx.surface, date_str, ViewPoint{date_x, row_y}, FontFace::PIZ_NUMB,
        dark_text, TextAnchor::Middle);

    // Guests
    context_.font->render_line_onto(
        *ctx.surface, compat::format("{}", stats.guests),
        ViewPoint{guests_x, row_y}, FontFace::PIZ_NUMB, dark_text,
        TextAnchor::End);
    total_guests += stats.guests;

    // Pizzas
    context_.font->render_line_onto(
        *ctx.surface, compat::format("{}", stats.orders_fulfilled),
        ViewPoint{pizzas_x, row_y}, FontFace::PIZ_NUMB, dark_text,
        TextAnchor::End);
    total_pizzas += stats.orders_fulfilled;

    // Sales
    context_.font->render_line_onto(
        *ctx.surface, compat::format("{}", stats.revenue),
        ViewPoint{sales_x, row_y}, FontFace::PIZ_NUMB, dark_text,
        TextAnchor::End);
    total_sales = total_sales + stats.revenue;

    // Profit
    context_.font->render_line_onto(
        *ctx.surface, compat::format("{}", stats.gross_profit),
        ViewPoint{profit_x, row_y}, FontFace::PIZ_NUMB, dark_text,
        TextAnchor::End);
    total_costs = total_costs + stats.ingredient_costs;

    // Percent
    int percent = stats.delivery_capacity_percent();
    context_.font->render_line_onto(*ctx.surface, compat::format("{}", percent),
                                    ViewPoint{percentage_x, row_y},
                                    FontFace::PIZ_NUMB, dark_text,
                                    TextAnchor::End);
    total_percent += percent;

    row_y += 20;
  }

  // Totals
  const int totals_y = 262;

  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", total_guests),
      ViewPoint{guests_x, totals_y}, FontFace::PIZ_NUMB, dark_text,
      TextAnchor::End);

  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", total_pizzas),
      ViewPoint{pizzas_x, totals_y}, FontFace::PIZ_NUMB, dark_text,
      TextAnchor::End);

  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", total_sales),
      ViewPoint{sales_x, totals_y}, FontFace::PIZ_NUMB, dark_text,
      TextAnchor::End);

  Money total_profit = total_sales - total_costs;
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", total_profit),
      ViewPoint{profit_x, totals_y}, FontFace::PIZ_NUMB, dark_text,
      TextAnchor::End);

  // Average percent
  int avg_percent = total_percent / 7;
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", avg_percent),
      ViewPoint{percentage_x, totals_y}, FontFace::PIZ_NUMB, dark_text,
      TextAnchor::End);
}

void RestaurantStats::render_month_review(const RenderContext& ctx) {
  render_background(ctx, true);
  constexpr uint8_t text_color = 240;

  // Header
  context_.font->render_line_onto(*ctx.surface, "monthly overview",
                                  ViewPoint{14, 40}, FontFace::PIZ_NUMB, 11,
                                  TextAnchor::Start);

  auto date = state_.time().current_date();
  std::string current_month{date.month_name()};
  std::string prev_month{date.previous_month_name()};

  context_.font->render_line_onto(*ctx.surface, prev_month, ViewPoint{184, 48},
                                  FontFace::PIZ_NUMB, 11, TextAnchor::Start);
  context_.font->render_line_onto(*ctx.surface, current_month,
                                  ViewPoint{292, 48}, FontFace::PIZ_NUMB, 11,
                                  TextAnchor::Start);
  gfx::draw::line(*ctx.surface, {14, 82}, {380, 82}, 13, 2);
  gfx::draw::line(*ctx.surface, {14, 176}, {380, 176}, 13, 2);
  gfx::draw::line(*ctx.surface, {14, 344}, {380, 344}, 13, 2);

  // Get financial data
  const auto& financials = state_.active_player()->financials();
  const int prev_month_col = 224;
  const int curr_month_col = 338;

  const int receipts_y = 66;
  const int sales_y = 84;
  const int coin_machines_y = 102;
  // Receipts section
  context_.font->render_line_onto(
      *ctx.surface, "receipts:", ViewPoint{20, receipts_y}, FontFace::PIZ_NUMB,
      text_color, TextAnchor::Start);

  // Sales (negate for display - revenue accounts are credits in accounting)
  context_.font->render_line_onto(
      *ctx.surface, "sales:", ViewPoint{30, sales_y}, FontFace::PIZ_NUMB,
      text_color, TextAnchor::Start);
  auto prev_sales =
      -financials.previous_month(MonthlyCategory::Sales, date, restaurant_id_);
  auto curr_sales =
      -financials.current_month(MonthlyCategory::Sales, date, restaurant_id_);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", prev_sales),
      ViewPoint{prev_month_col, sales_y}, FontFace::PIZ_NUMB, text_color,
      TextAnchor::End);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", curr_sales),
      ViewPoint{curr_month_col, sales_y}, FontFace::PIZ_NUMB, text_color,
      TextAnchor::End);

  // Coin machines (negated)
  context_.font->render_line_onto(
      *ctx.surface, "coin machines:", ViewPoint{30, coin_machines_y},
      FontFace::PIZ_NUMB, text_color, TextAnchor::Start);
  auto prev_coins = -financials.previous_month(MonthlyCategory::CoinMachines,
                                               date, restaurant_id_);
  auto curr_coins = -financials.current_month(MonthlyCategory::CoinMachines,
                                              date, restaurant_id_);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", prev_coins),
      ViewPoint{prev_month_col, coin_machines_y}, FontFace::PIZ_NUMB,
      text_color, TextAnchor::End);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", curr_coins),
      ViewPoint{curr_month_col, coin_machines_y}, FontFace::PIZ_NUMB,
      text_color, TextAnchor::End);

  // Total receipts (both already negated)
  auto prev_receipts = prev_sales + prev_coins;
  auto curr_receipts = curr_sales + curr_coins;
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", prev_receipts),
      ViewPoint{prev_month_col, receipts_y}, FontFace::PIZ_NUMB, text_color,
      TextAnchor::End);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", curr_receipts),
      ViewPoint{curr_month_col, receipts_y}, FontFace::PIZ_NUMB, text_color,
      TextAnchor::End);

  // Expenditure
  const int expenditure_y = 160;
  context_.font->render_line_onto(
      *ctx.surface, "expenditure:", ViewPoint{20, expenditure_y},
      FontFace::PIZ_NUMB, text_color, TextAnchor::Start);

  const std::vector<MonthlyCategory> exp_categories = {
      MonthlyCategory::Staff,     MonthlyCategory::Rents,
      MonthlyCategory::Stock,     MonthlyCategory::Machines,
      MonthlyCategory::Furniture, MonthlyCategory::Protection};
  const std::vector<std::string> exp_labels = {
      "staff:", "rents:", "stock:", "machines:", "furniture:", "protection:"};

  const int line_height = 20;
  int y = expenditure_y + line_height;
  Money prev_total_exp{0};
  Money curr_total_exp{0};

  for (size_t i = 0; i < exp_categories.size(); ++i) {
    context_.font->render_line_onto(*ctx.surface, exp_labels[i],
                                    ViewPoint{30, y}, FontFace::PIZ_NUMB,
                                    text_color, TextAnchor::Start);

    auto prev_exp =
        financials.previous_month(exp_categories[i], date, restaurant_id_);
    auto curr_exp =
        financials.current_month(exp_categories[i], date, restaurant_id_);
    prev_total_exp = prev_total_exp + prev_exp;
    curr_total_exp = curr_total_exp + curr_exp;

    context_.font->render_line_onto(
        *ctx.surface, compat::format("{}", prev_exp),
        ViewPoint{prev_month_col, y}, FontFace::PIZ_NUMB, text_color,
        TextAnchor::End);
    context_.font->render_line_onto(
        *ctx.surface, compat::format("{}", curr_exp),
        ViewPoint{curr_month_col, y}, FontFace::PIZ_NUMB, text_color,
        TextAnchor::End);
    y += line_height;
  }

  // Total expenditure
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", prev_total_exp),
      ViewPoint{prev_month_col, expenditure_y}, FontFace::PIZ_NUMB, text_color,
      TextAnchor::End);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", curr_total_exp),
      ViewPoint{curr_month_col, expenditure_y}, FontFace::PIZ_NUMB, text_color,
      TextAnchor::End);

  const int result_y = 326;

  // Result
  context_.font->render_line_onto(
      *ctx.surface, "result:", ViewPoint{20, result_y}, FontFace::PIZ_NUMB,
      text_color, TextAnchor::Start);
  auto prev_result = prev_receipts - prev_total_exp;
  auto curr_result = curr_receipts - curr_total_exp;
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", prev_result),
      ViewPoint{prev_month_col, result_y}, FontFace::PIZ_NUMB, text_color,
      TextAnchor::End);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("{}", curr_result),
      ViewPoint{curr_month_col, result_y}, FontFace::PIZ_NUMB, text_color,
      TextAnchor::End);
}

void RestaurantStats::render_customer_stats(const RenderContext& ctx) {
  auto box = ViewRect{0, 0, views::base::MAIN_PANEL_WIDTH,
                      views::base::MAIN_PANEL_HEIGHT};
  views::ui::render_corner_decorations(
      *ctx.surface, context_.blitmap->get<Blitmap2Icon>(), box);

  const int edge_offset = 6;
  views::ui::render_decorated_corners(
      *ctx.surface, stanbobs_,
      ViewRect{{edge_offset, edge_offset},
               {views::base::MAIN_PANEL_WIDTH - (edge_offset * 2),
                views::base::MAIN_PANEL_HEIGHT - (edge_offset * 2)}},
      4, {11});

  ctx.surface->palette(PaletteHandle{7});

  context_.font->render_line_onto(*ctx.surface, "Customers", ViewPoint{192, 24},
                                  FontFace::RUBY, 12, TextAnchor::Middle);

  // Get customer data
  // potential = raw customers from location (yellow bar)
  // current = after competition from economy (green bar)
  const auto& potential = restaurant_->property().potential_customers();
  const auto& current = restaurant_->economy().customer_counts();

  const auto& stabo = context_.bob->get("bob/stabo.bob");
  const auto& yellow_bar = stabo.frame(11);
  const auto& green_bar = stabo.frame(10);

  const auto& blitmap = context_.blitmap->get<Blitmap2Icon>();

  using LegacyConstants::WOOD_BORDER_HEIGHT_TOP;
  using LegacyConstants::WOOD_BORDER_WIDTH_LEFT;

  const int demographic_spacing = 26 * 2;
  const int icon_y = (158 * 2) - WOOD_BORDER_HEIGHT_TOP;
  const int start_x_offset = (31 * 2) - WOOD_BORDER_WIDTH_LEFT + 20;
  const int yellow_bar_start_y = (148 * 2) - WOOD_BORDER_HEIGHT_TOP;
  const int green_bar_start_y = (150 * 2) - WOOD_BORDER_HEIGHT_TOP;
  const int min_y_for_bars = (40 * 2) - WOOD_BORDER_HEIGHT_TOP;
  const int min_text_y = (36 * 2) - WOOD_BORDER_HEIGHT_TOP;

  // X offsets for numbers
  const int top_number_x_offset = (54 - 31) * 2;
  const int bottom_number_x_offset = (50 - 31) * 2;

  // Demographic icons in order
  const std::array<Blitmap2Icon, 6> demo_icons = {
      Blitmap2Icon::CHILDREN,    Blitmap2Icon::YOUNG_PEOPLE,
      Blitmap2Icon::BLUE_COLLAR, Blitmap2Icon::WHITE_COLLAR,
      Blitmap2Icon::BUSINESS,    Blitmap2Icon::SENIORS};

  // Draw each demographic
  for (auto demo : ALL_CONSUMER_TYPES) {
    int idx = static_cast<int>(demo);
    const int column_x = start_x_offset + (idx * demographic_spacing);

    // Get values and calculate bar heights
    const int potential_value = potential[demo];
    const int current_value = current[demo];
    const int potential_height = potential_value / 4;
    const int current_height = current_value / 4;

    // Draw demographic icon at bottom
    const auto& icon = blitmap.icon_surface(demo_icons[idx]);
    ctx.surface->blit_masked(icon, ViewPoint{column_x - 20, icon_y});

    // Draw yellow bar (potential customers) - stacked sprites going upward
    // Start from bottom and move upward, stopping at min_y_for_bars or after
    // height pixels. Always draw at least one frame
    {
      int bar_y = yellow_bar_start_y - 8;
      int iterations = std::max(1, potential_height);
      for (int h = 0; h < iterations && bar_y >= min_y_for_bars; ++h) {
        ctx.surface->blit_masked(yellow_bar, ViewPoint{column_x, bar_y});
        bar_y -= 2; // Move up 2 pixels (1 pixel in original, 2x scale)
      }
    }

    // Calculate Y position for top number (potential)
    int top_number_y = yellow_bar_start_y - (potential_height * 2) - 8 -
                       WOOD_BORDER_HEIGHT_TOP + 16;
    top_number_y = std::max(top_number_y, min_text_y);

    // Render potential customer count (above yellow bar)
    const int top_number_x =
        column_x + top_number_x_offset - WOOD_BORDER_WIDTH_LEFT + 4;
    context_.font->render_line_onto(*ctx.surface,
                                    compat::format("{}", potential_value),
                                    ViewPoint{top_number_x, top_number_y},
                                    FontFace::SUPERSMA, 240, TextAnchor::End);

    {
      // Draw green bar (actual customers) - stacked sprites going upward
      int bar_y = green_bar_start_y - 8;
      int iterations = std::max(1, current_height);
      for (int h = 0; h < iterations && bar_y >= min_y_for_bars; ++h) {
        ctx.surface->blit_masked(green_bar, ViewPoint{column_x, bar_y});
        bar_y -= 2; // Move up 2 pixels
      }
    }

    // Render actual customer count (between bars and icon)
    const int bottom_number_x =
        column_x + bottom_number_x_offset - WOOD_BORDER_WIDTH_LEFT + 4;
    const int bottom_number_y =
        green_bar_start_y + 12 - WOOD_BORDER_HEIGHT_TOP + 18;
    context_.font->render_line_onto(*ctx.surface,
                                    compat::format("{}", current_value),
                                    ViewPoint{bottom_number_x, bottom_number_y},
                                    FontFace::SUPERSMA, 240, TextAnchor::End);
  }
}

void RestaurantStats::render_sales_curve(const RenderContext& ctx) {
  auto box = ViewRect{0, 0, views::base::MAIN_PANEL_WIDTH,
                      views::base::MAIN_PANEL_HEIGHT};
  views::ui::render_corner_decorations(
      *ctx.surface, context_.blitmap->get<Blitmap2Icon>(), box);

  constexpr int edge_offset = 6;
  views::ui::render_decorated_corners(
      *ctx.surface, stanbobs_,
      ViewRect{{edge_offset, edge_offset},
               {views::base::MAIN_PANEL_WIDTH - (edge_offset * 2),
                views::base::MAIN_PANEL_HEIGHT - (edge_offset * 2)}},
      4, {11});

  // Header layout
  constexpr int title_x = 84;
  constexpr int title_y = 10;
  constexpr int scale_text_x = 130;
  constexpr int scale_text_y = 36;

  // Axis graphic positions
  constexpr int x_axis_x = 52 - 16;
  constexpr int x_axis_y = 254;
  constexpr int y_axis_x = 28 - 16;
  constexpr int y_axis_y = 44;

  // Bar chart layout
  constexpr int bar_start_x = 66;
  constexpr int bar_spacing = 42;
  constexpr int bar_base_y = 254;
  constexpr int bar_min_y = 64;
  constexpr int bar_step = 2;

  // Expenditure bars
  constexpr int expenditure_bar_offset_x = 14;
  constexpr int expenditure_bar_offset_y = -4;

  constexpr int day_label_y = 288;
  constexpr int header_color = 13;
  constexpr int label_color = 240;

  ctx.surface->palette(PaletteHandle{7});

  // Collect last 7 days of data, padding with empty days if needed
  auto week_data = restaurant_->stats().last_week();
  while (week_data.size() < 7) {
    week_data.insert(week_data.begin(), core::restaurant::DailyStats{});
  }
  if (week_data.size() > 7) {
    week_data.erase(week_data.begin(),
                    week_data.begin() + static_cast<int>(week_data.size() - 7));
  }

  // Calculate daily purchases
  auto get_daily_purchases = [](const core::restaurant::DailyStats& day) {
    Money total{0};
    for (const auto& cat : day.ingredient_purchases_by_category) {
      total = total + cat;
    }
    total = total + day.furniture_purchases;
    return total;
  };

  // BUG COMPATIBILITY: Original game shows today's purchases on yesterday's
  // bar. When you buy something today, it appears immediately but is
  // incorrectly attributed to yesterday. Set to false to fix this bug and show
  // purchases on the correct day.
  bool keep_original_display_bug = !state_.rules().fix_legacy_bugs;

  // Get today's purchases for the bug-compatible display
  const auto& today_stats = restaurant_->stats().today;
  Money today_purchases = get_daily_purchases(today_stats);

  std::vector<int64_t> values;
  values.reserve((week_data.size() * 2) + 1);

  for (const auto& day : week_data) {
    values.push_back(day.revenue.cents / 100);
    values.push_back(get_daily_purchases(day).cents / 100);
  }

  if (keep_original_display_bug) {
    values.push_back(today_purchases.whole());
  }

  int scale = compute_scale(values.begin(), values.end(), 100);

  // Draw axis graphics (bob frame 0 = X-axis, frame 1 = Y-axis)
  const auto& stabo = context_.bob->get("bob/stabo.bob");
  ctx.surface->blit_masked(stabo.frame(0), ViewPoint{x_axis_x, x_axis_y});
  ctx.surface->blit_masked(stabo.frame(1), ViewPoint{y_axis_x, y_axis_y});

  // Render title and scale
  context_.font->render_line_onto(*ctx.surface, "Sales curve",
                                  ViewPoint{title_x, title_y}, FontFace::RUBY,
                                  header_color, TextAnchor::Start);
  context_.font->render_line_onto(
      *ctx.surface, compat::format("in ${} units", scale * 10),
      ViewPoint{scale_text_x, scale_text_y}, FontFace::PIZ_NUMB, header_color,
      TextAnchor::Start);

  // Bar sprites: frame 10 = green (sales), frame 12 = red (expenditure)
  const auto& sales_bar = stabo.frame(10);
  const auto& expenditure_bar = stabo.frame(12);

  auto current_date = state_.time().current_date();

  for (int i = 0; i < 7; ++i) {
    const auto& day = week_data[i];
    int x = bar_start_x + (i * bar_spacing);

    // Draw expenditure bars (red) - purchases at suppliers
    // BUG: Original game shows today's purchases on yesterday's bar (i=6).
    // This means when you buy something, it appears immediately but attributed
    // to the wrong day. Controlled by use_original_purchase_display_bug.
    int purchases_dollars;
    if (keep_original_display_bug && i == 6) {
      // Bug behavior: yesterday's bar shows TODAY's purchases
      purchases_dollars = static_cast<int>(today_purchases.cents / 100);
    }
    else if (keep_original_display_bug) {
      // Bug behavior: other bars show next day's historical data
      int purchase_day_idx = i + 1;
      purchases_dollars = static_cast<int>(
          get_daily_purchases(week_data[purchase_day_idx]).cents / 100);
    }
    else {
      // Fixed behavior: each bar shows its own day's purchases
      purchases_dollars =
          static_cast<int>(get_daily_purchases(day).cents / 100);
    }
    int purchases_height = purchases_dollars / scale;
    for (int h = 0, y = bar_base_y + expenditure_bar_offset_y;
         h < purchases_height && y >= bar_min_y; ++h, y -= bar_step) {
      ctx.surface->blit_masked(expenditure_bar,
                               ViewPoint{x + expenditure_bar_offset_x, y});
    }

    // Draw sales bars (green) - in front of expenditure bars
    int revenue_dollars = static_cast<int>(day.revenue.cents / 100);
    int revenue_height = revenue_dollars / scale;
    for (int h = 0, y = bar_base_y; h < revenue_height && y >= bar_min_y;
         ++h, y -= bar_step) {
      ctx.surface->blit_masked(sales_bar, ViewPoint{x, y});
    }

    // Day label
    auto date = current_date.add_days(-(7 - i));
    context_.font->render_line_onto(
        *ctx.surface, compat::format("{}-{}", date.day(), date.month()),
        ViewPoint{x, day_label_y}, FontFace::SUPERSMA, label_color,
        TextAnchor::Start);
  }
}

void RestaurantStats::navigate_to_restaurant(core::RestaurantId new_id) {
  state_.active_restaurant(new_id);
  std::optional<int> screen_idx;
  if (current_screen_ != RestaurantStatsScreen::None) {
    screen_idx = static_cast<int>(current_screen_);
  }
  state_.ui().replace_top<RestaurantStatsState>(RestaurantStatsState{
      .id = new_id,
      .screen_index = screen_idx,
      .automatic_mode = automatic_mode_,
  });
}
