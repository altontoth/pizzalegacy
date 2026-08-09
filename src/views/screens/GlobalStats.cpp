/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/GlobalStats.h"
#include "core/character/Actor.h"
#include "core/character/Player.h"
#include "core/city/CityNames.h"
#include "core/city/District.h"
#include "core/city/Property.h"
#include "core/finance/BalanceSheet.h"
#include "core/finance/Scale.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "engine/ui/TextAnchor.h"
#include "gfx/Draw.h"
#include "resources/BlitmapManager.h"
#include "views/base/ViewDefs.h"
#include "views/ui/PageDecoration.h"
#include "views/ui/finance/FinanceComponents.h"

constexpr int text_color = 11;

namespace {
// Helper functions removed - now query ledger directly
} // namespace

GlobalStats::GlobalStats(GameContext& context, GameState& state)
    : MainPanelView(context, state),
      stanbobs_(context_.bob->get("bob/stanbobs.bob")),
      blitmap2_(context_.blitmap->get<Blitmap2Icon>()) {

  create_buttons();
}

void GlobalStats::tick() {}

void GlobalStats::render(const RenderContext& ctx) {
  ctx.surface->palette(PaletteHandle{8});
  interactions_.begin_frame();
  uint8_t bg_color = 21;
  ctx.surface->fill(bg_color);

  switch (current_screen_) {
    break;
  case GlobalStatsScreen::Month_Overview:
    render_background(ctx, true);
    render_month_overview(ctx);
    break;
  case GlobalStatsScreen::Cash_Flow:
    render_cash_flow(ctx);
    break;
  case GlobalStatsScreen::Loans:
    render_loans(ctx);
    break;
  case GlobalStatsScreen::Insurance:
    render_insurance(ctx);
    break;
  case GlobalStatsScreen::Property:
    render_property(ctx);
    break;
  case GlobalStatsScreen::Balance:
    render_balance(ctx);
    break;
  case GlobalStatsScreen::Daily_Profits:
    render_daily_profits(ctx);
    break;
  case GlobalStatsScreen::Restaurants:
    // Has its own state
    [[fallthrough]];
  case GlobalStatsScreen::None:
    break;
  }
}

void GlobalStats::create_buttons() {
  using GS = GlobalStatsScreen;

  auto make_screen_button = [this](GS screen, Icon icon, ButtonPosition pos,
                                   const std::string& tooltip) {
    input_panel_->add_button(
        icon, ButtonType::Square, pos, tooltip, [this, screen]() {
          if (screen == GS::None) {
            state_.ui().pop();
          }
          else if (screen == GS::Restaurants) {
            state_.ui().push<StatsRestaurantsState>(
                state_.active_player()->id());
          }
          else {
            current_screen_ = screen;
            mark_dirty();
            Logger::debug("Switched to screen: {}", static_cast<int>(screen));
          }
        });
  };

  make_screen_button(GS::Month_Overview, Icon::CALCULATOR,
                     ButtonPosition::TopLeft, "Month Overview");
  make_screen_button(GS::Restaurants, Icon::RESTAURANT,
                     ButtonPosition::TopCenter, "Restaurants");
  make_screen_button(GS::Cash_Flow, Icon::MONEY_TAP, ButtonPosition::TopRight,
                     "Cash Flow");
  make_screen_button(GS::Loans, Icon::MONEY_BAG, ButtonPosition::MiddleLeft,
                     "Loans");
  make_screen_button(GS::Insurance, Icon::PADLOCK, ButtonPosition::MiddleCenter,
                     "Insurance");
  make_screen_button(GS::Property, Icon::HOUSE, ButtonPosition::MiddleRight,
                     "Property");
  make_screen_button(GS::Balance, Icon::BALANCE, ButtonPosition::BottomLeft,
                     "Balance");
  make_screen_button(GS::Daily_Profits, Icon::GRAPH,
                     ButtonPosition::BottomCenter, "Daily profits");
  make_screen_button(GS::None, Icon::ARROW_BACK, ButtonPosition::BottomRight,
                     "Back");
}

void GlobalStats::render_binder(const RenderContext& ctx) {
  // Get all the binder pieces
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

void GlobalStats::render_background(const RenderContext& ctx, bool show_grid) {
  ctx.surface->fill(29);
  if (show_grid) {
    constexpr uint8_t GRID_COLOR = 27;
    gfx::draw::grid(*ctx.surface,
                    {0, 0, ctx.surface->width(), ctx.surface->height()},
                    GRID_COLOR);
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

void GlobalStats::render_month_overview(const RenderContext& ctx) {
  render_background(ctx, true);

  auto date = state_.time().current_date();
  views::ui::finance::render_month_header(*ctx.surface, *context_.font, date,
                                          "monthly overview");

  int line_color = 13;
  int line_width = 2;

  // Horizontal divider lines
  gfx::draw::line(*ctx.surface, {14, 88}, {376, 88}, line_color, line_width);
  gfx::draw::line(*ctx.surface, {14, 176}, {376, 176}, line_color, line_width);
  gfx::draw::line(*ctx.surface, {14, 346}, {376, 346}, line_color, line_width);

  auto& financials = state_.active_player()->financials();
  auto restaurants = state_.restaurants_for(state_.active_player()->id());

  // Value columns, right-aligned against the panel edge.
  const int last_month_x = ctx.surface->width() - 146;
  const int this_month_x = ctx.surface->width() - 30;
  constexpr int line_spacing = 18;
  constexpr uint8_t item_color = 240;

  // Lambda for printing an individual line
  auto draw_label_and_amounts = [&](std::string_view label, Money last_m,
                                    Money this_m, ViewPoint pos) {
    context_.font->render_text_onto(*ctx.surface, compat::format("{}:", label),
                                    pos, FontFace::PIZ_NUMB, item_color);
    context_.font->render_text_onto(*ctx.surface, compat::format("{}", last_m),
                                    ViewPoint{last_month_x, pos.y},
                                    FontFace::PIZ_NUMB, item_color,
                                    TextAnchor::End);
    context_.font->render_text_onto(*ctx.surface, compat::format("{}", this_m),
                                    ViewPoint{this_month_x, pos.y},
                                    FontFace::PIZ_NUMB, item_color,
                                    TextAnchor::End);
  };

  // Clickable row dimensions (original coords * 2)
  constexpr int row_x = 18;
  constexpr int row_width = 340;
  constexpr int row_height = 18;

  // Lambda to create a clickable row
  auto create_clickable_row = [&](Account acct, ViewPoint pos, bool is_receipt,
                                  bool is_total, const std::string& label) {
    ViewRect click_area{row_x, pos.y - 2, row_width, row_height};
    interactions_.create_frame_item(
        click_area, label, [this, acct, is_receipt, is_total](auto& ie) {
          if (ie.type == views::ui::InteractionEvent::Type::Click &&
              ie.button == views::ui::MouseButton::Left) {
            popup_state_.visible = true;
            popup_state_.account = acct;
            popup_state_.is_receipts = is_receipt;
            popup_state_.show_category_total = is_total;
            popup_state_.scroll_offset = 0;
            mark_dirty();
          }
        });
  };

  // Draw receipts section with clickable rows
  // Receipts are credits (negative in ledger); negate for display.
  Money receipts_this = 0_usd;
  Money receipts_last = 0_usd;
  {
    ViewPoint pos{18, 72};

    // Current month is summed per restaurant (so the drill-down matches);
    // previous month is a per-player figure read with one player-level query.
    for (auto acct : PlayerFinancials::receipt_accounts()) {
      receipts_last += financials.previous_month_operational(acct, date);
      for (auto* r : restaurants) {
        receipts_this +=
            financials.current_month_operational(acct, date, r->id());
      }
    }
    draw_label_and_amounts("receipts", -receipts_last, -receipts_this, pos);
    create_clickable_row(Account::Sales, pos, true, true, "receipts_total");

    for (auto acct : PlayerFinancials::receipt_accounts()) {
      pos.y += line_spacing;
      Money this_m = 0_usd;
      Money last_m = financials.previous_month_operational(acct, date);
      for (auto* r : restaurants) {
        this_m += financials.current_month_operational(acct, date, r->id());
      }
      draw_label_and_amounts(to_string(acct), -last_m, -this_m, pos);
      create_clickable_row(acct, pos, true, false, to_string(acct));
    }
  }

  // Draw expenditure section with clickable rows
  Money exp_this = 0_usd;
  Money exp_last = 0_usd;
  {
    ViewPoint pos{18, 160};

    for (auto acct : PlayerFinancials::expenditure_accounts()) {
      exp_last += financials.previous_month_operational(acct, date);
      for (auto* r : restaurants) {
        exp_this += financials.current_month_operational(acct, date, r->id());
      }
    }
    draw_label_and_amounts("expenditure", exp_last, exp_this, pos);
    create_clickable_row(Account::Staff, pos, false, true, "expenditure_total");

    for (auto acct : PlayerFinancials::expenditure_accounts()) {
      pos.y += line_spacing;
      Money this_m = 0_usd;
      Money last_m = financials.previous_month_operational(acct, date);
      for (auto* r : restaurants) {
        this_m += financials.current_month_operational(acct, date, r->id());
      }
      draw_label_and_amounts(to_string(acct), last_m, this_m, pos);
      create_clickable_row(acct, pos, false, false, to_string(acct));
    }
  }

  // Result = income - expenses (both displayed as positive)
  draw_label_and_amounts("result", -receipts_last - exp_last,
                         -receipts_this - exp_this, ViewPoint{18, 330});

  // Render popup if visible
  if (popup_state_.visible && !restaurants.empty()) {
    render_month_overview_popup(ctx, restaurants);
  }
}

void GlobalStats::render_month_overview_popup(
    const RenderContext& ctx, const std::vector<Restaurant*>& restaurants) {
  int visible_count = std::min(static_cast<int>(restaurants.size()),
                               MonthlyPopupState::max_visible);
  int total_restaurants = static_cast<int>(restaurants.size());

  // Popup dimensions
  constexpr int row_height = 20;
  constexpr int header_height = 40;
  int popup_height = header_height + (visible_count * row_height) + 10;
  ViewRect popup_rect{74, 74, 300, popup_height};

  // drop shadow:
  ctx.surface->fill_rect(
      {popup_rect.point() + PxOffset{-8, 10}, popup_rect.size()}, 14);
  // Draw popup background with border
  ctx.surface->fill_rect(popup_rect, 11); // Dark border

  // Draw inner area
  ViewRect inner_rect{popup_rect.x + 2, popup_rect.y + 2, popup_rect.w - 4,
                      popup_rect.h - 4};
  ctx.surface->fill_rect(inner_rect, 15); // Light inner
  gfx::draw::line(*ctx.surface, popup_rect.point() + PxOffset{0, 30},
                  popup_rect.point() + PxOffset{298, 30}, 11, 4);

  constexpr uint8_t text_color = 11;

  const PxOffset header_offset{4, 4};
  // Draw header: "restaurants"
  context_.font->render_text_onto(*ctx.surface, "restaurants",
                                  popup_rect.point() + header_offset,
                                  FontFace::PIZ_NUMB, text_color);

  // Draw account/category name
  std::string account_label;
  if (popup_state_.show_category_total) {
    account_label = popup_state_.is_receipts ? "receipts:" : "expenditure:";
  }
  else {
    account_label = compat::format("{}:", to_string(popup_state_.account));
  }
  const PxOffset label_offset{162, 10};
  context_.font->render_text_onto(*ctx.surface, account_label,
                                  popup_rect.point() + label_offset,
                                  FontFace::PIZ_NUMB, text_color);

  // Read values from restaurant's monthly accounting (separate from ledger)
  int y = header_height + 2;

  for (int i = 0; i < visible_count; ++i) {
    int idx = popup_state_.scroll_offset + i;
    if (idx >= total_restaurants) {
      break;
    }

    auto* restaurant = restaurants[idx];
    auto& financials = state_.active_player()->financials();
    const auto current_date = state_.time().current_date();
    Money value = 0_usd;

    if (popup_state_.show_category_total) {
      // Sum all receipt or expenditure accounts for this restaurant
      const auto accounts = popup_state_.is_receipts
                                ? PlayerFinancials::receipt_accounts()
                                : PlayerFinancials::expenditure_accounts();
      for (auto acct : accounts) {
        value += financials.current_month_operational(acct, current_date,
                                                      restaurant->id());
      }
    }
    else {
      // Single account for this restaurant
      value = financials.current_month_operational(
          popup_state_.account, current_date, restaurant->id());
    }

    // Receipts are credits (negative in ledger); negate for display
    if (popup_state_.is_receipts) {
      value = -value;
    }

    const PxOffset rest_name_offset{4, y};
    const PxOffset rest_value_offset{popup_rect.w - 10, y};
    // Restaurant name
    context_.font->render_text_onto(*ctx.surface, restaurant->name(),
                                    popup_rect.point() + rest_name_offset,
                                    FontFace::PIZ_NUMB, text_color);

    // Value (right-aligned)
    context_.font->render_text_onto(*ctx.surface, compat::format("{}", value),
                                    popup_rect.point() + rest_value_offset,
                                    FontFace::PIZ_NUMB, text_color,
                                    TextAnchor::End);

    y += row_height;
  }

  // Draw scroll indicator if there are more restaurants
  if (total_restaurants > MonthlyPopupState::max_visible) {
    int max_scroll = total_restaurants - MonthlyPopupState::max_visible;
    std::string scroll_info = compat::format(
        "{}-{} of {}", popup_state_.scroll_offset + 1,
        popup_state_.scroll_offset + visible_count, total_restaurants);
    const PxOffset info_offset{popup_rect.w / 2, popup_rect.h - 10};
    context_.font->render_text_onto(
        *ctx.surface, scroll_info, popup_rect.point() + info_offset,
        FontFace::MINI, text_color, TextAnchor::Middle);
  }

  // Create backdrop to dismiss popup when clicking outside
  interactions_.create_frame_item(
      ViewRect{0, 0, views::base::MAIN_PANEL_WIDTH,
               views::base::MAIN_PANEL_HEIGHT},
      "monthly_popup_backdrop",
      [this](auto& ie) {
        if (ie.type == views::ui::InteractionEvent::Type::Click &&
            ie.button == views::ui::MouseButton::Left) {
          popup_state_.visible = false;
          popup_state_.scroll_offset = 0;
          mark_dirty();
        }
      },
      {}, // no tooltip
      views::ui::InteractionLayer::Exclusive);

  // Create interaction for the popup (handles scroll and dismiss)
  interactions_.create_frame_item(
      popup_rect, "monthly_popup",
      [this, total_restaurants](auto& ie) {
        if (ie.type == views::ui::InteractionEvent::Type::Scroll) {
          int max_scroll =
              std::max(0, total_restaurants - MonthlyPopupState::max_visible);
          if (ie.scroll_delta > 0 && popup_state_.scroll_offset > 0) {
            popup_state_.scroll_offset--;
            mark_dirty();
          }
          else if (ie.scroll_delta < 0 &&
                   popup_state_.scroll_offset < max_scroll) {
            popup_state_.scroll_offset++;
            mark_dirty();
          }
          return;
        }
        if (ie.type == views::ui::InteractionEvent::Type::Click &&
            ie.button == views::ui::MouseButton::Left) {
          popup_state_.visible = false;
          popup_state_.scroll_offset = 0;
          mark_dirty();
        }
      },
      {}, // no tooltip
      views::ui::InteractionLayer::Exclusive);
}

void GlobalStats::render_bar_chart(
    const RenderContext& ctx, const std::vector<int64_t>& data,
    std::string_view title,
    std::string_view
        x_axis_label) { // NOLINT(readability-convert-member-functions-to-static)
  ctx.surface->palette({43});
  ctx.surface->fill(29);

  // The original allows bars to reach at most 100 scaled units; this bound
  // determines the scale factor (and thus the "in $X units" label).
  constexpr int max_scaled_units = 100;
  // One scaled unit = 2 pixels in our 2x coordinate space vs. 1 in the
  // original.
  constexpr int pixels_per_scaled_unit = 2;
  int scale = compute_scale(data.begin(), data.end(), max_scaled_units);

  constexpr int edge_offset = 6;
  views::ui::render_decorated_corners(
      *ctx.surface, stanbobs_,
      ViewRect{{edge_offset, edge_offset},
               {views::base::MAIN_PANEL_WIDTH - (edge_offset * 2),
                views::base::MAIN_PANEL_HEIGHT - (edge_offset * 2)}},
      4, {11});

  TextStyle ts{
      .color = 12,
      .align = TextAnchor::Middle,
  };
  context_.font->render_text_onto(
      *ctx.surface, std::string(title),
      ViewPoint{views::base::MAIN_PANEL_WIDTH / 2, 14}, FontFace::RUBY, ts);

  ts.color = 240;
  context_.font->render_text_onto(
      *ctx.surface, compat::format("in ${} units", scale * 10),
      ViewPoint{views::base::MAIN_PANEL_WIDTH / 2, 38}, FontFace::PIZ_NUMB, ts);

  constexpr int x_axis_x = 38;
  constexpr int x_axis_y = 240;
  constexpr int y_axis_x = 14;
  constexpr int y_axis_y = 30;

  // Draw axis graphics (bob frame 0 = X-axis, frame 1 = Y-axis)
  const auto& stabo = context_.bob->get("bob/stabo.bob");
  ctx.surface->blit_masked(stabo.frame(0), ViewPoint{x_axis_x, x_axis_y});
  ctx.surface->blit_masked(stabo.frame(1), ViewPoint{y_axis_x, y_axis_y});
  // X-axis label
  context_.font->render_text_onto(*ctx.surface, x_axis_label,
                                  ViewPoint{376, 266}, FontFace::CSTANDAR, 13,
                                  TextAnchor::End);

  constexpr int bottom_y = 258;
  constexpr int x_start = 48;
  constexpr int column_spacing = 6;

  constexpr std::array<int, 6> bar_colors = {9, 26, 28, 30, 31, 24};
  constexpr std::array<int, 7> thresholds = {
      0, 10'000, 100'000, 500'000, 1'000'000, 5'000'000, 10'000'000};

  constexpr int loss_color = 23;
  constexpr int max_loss_height = 20 * pixels_per_scaled_unit;
  auto scaled_px = [&](int64_t v) {
    return static_cast<int>(v / scale) * pixels_per_scaled_unit;
  };

  int x = x_start;
  for (auto value : data) {
    if (value < 0) {
      int bar_height = std::min(scaled_px(-value), max_loss_height);
      if (bar_height > 0) {
        gfx::draw::line(*ctx.surface, {x, bottom_y}, {x, bottom_y + bar_height},
                        loss_color, 4);
      }
    }
    else {
      int cur_y = bottom_y;
      for (size_t i = 0; i < bar_colors.size(); ++i) {
        int upper = thresholds[i + 1];

        if (value < upper) {
          // partial segment
          int top_y = bottom_y - scaled_px(value);
          if (cur_y > top_y) {
            gfx::draw::line(*ctx.surface, {x, cur_y}, {x, top_y}, bar_colors[i],
                            4);
          }
          break;
        }

        // full segment
        int next_y = bottom_y - scaled_px(upper);
        if (cur_y > next_y) {
          gfx::draw::line(*ctx.surface, {x, cur_y}, {x, next_y}, bar_colors[i],
                          4);
          cur_y = next_y;
        }
      }
    }

    x += column_spacing;
  }
}

void GlobalStats::render_cash_flow(const RenderContext& ctx) {
  auto& financials = state_.active_player()->financials();
  auto today = state_.time().current_date();

  // Show current year's weekly bars. Each bar's value is the true running cash
  // balance queried from game_start_date, so initial capital is included.
  std::vector<int64_t> weekly_dollars;
  GameDate cursor = today.start_of_year();
  while (cursor < today) {
    GameDate next = cursor.add_days(7);
    Money balance =
        financials.balance_between(Account::Cash, game_start_date, next);
    weekly_dollars.push_back(balance.whole());
    cursor = next;
  }

  render_bar_chart(ctx, weekly_dollars, "liquidity", "weeks");
}

void GlobalStats::render_daily_profits(const RenderContext& ctx) {
  auto& financials = state_.active_player()->financials();

  // The rolling series holds the most recent days at the end; draw only the
  // filled entries.
  const auto& history = financials.daily_profit_history();
  const int days = financials.daily_profit_days();
  std::vector<int64_t> daily_dollars(history.end() - days, history.end());

  render_bar_chart(ctx, daily_dollars, "daily profits", "days");
}

void GlobalStats::render_loans(const RenderContext& ctx) {
  render_background(ctx, false);

  /* I've moved some columns about 20 pixels to the left to allow for the
     dollar sign and in general have a bit more room. */
  constexpr int header_y = 54;
  context_.font->render_text_onto(*ctx.surface, "loans", {12, 58},
                                  FontFace::ASTANDAR);
  context_.font->render_text_onto(*ctx.surface, "amount", {154, header_y},
                                  FontFace::PIZ_NUMB, 240);
  context_.font->render_text_onto(*ctx.surface, "time", {222, header_y},
                                  FontFace::PIZ_NUMB, 240);
  context_.font->render_text_onto(*ctx.surface, "%", {264, header_y},
                                  FontFace::PIZ_NUMB, 240);
  context_.font->render_text_onto(*ctx.surface, "$/month", {312, header_y},
                                  FontFace::PIZ_NUMB, 240);

  const Player* player = state_.active_player();
  auto cities = state_.visited_cities(*player);

  constexpr int col_city = 4;
  constexpr int col_amount = 218;
  constexpr int col_time = 246;
  constexpr int col_pct = 276;
  constexpr int col_month = 380;

  struct LoanEntry {
    core::CityId city_id;
    const Loan* loan;
  };
  std::vector<LoanEntry> active_loans;
  for (auto* city : cities) {
    for (const auto& loan : player->loans_in_city(city->id())) {
      if (!loan.is_settled()) {
        active_loans.push_back({city->id(), &loan});
      }
    }
  }

  auto active_city_id = state_.active_city_id();

  constexpr int loan_row_y0 = 92;
  constexpr int loan_row_height = 20;
  for (int i = 0; i < std::min(4, (int)active_loans.size()); i++) {
    const auto& [city_id, loan] = active_loans[i];
    int y = loan_row_y0 + (i * loan_row_height);

    if (city_id == active_city_id) {
      const auto* city = state_.city(city_id);
      const Actor* bank =
          city ? city->actor_by_id(static_cast<uint8_t>(loan->bank_id().value))
               : nullptr;
      std::string label = bank ? bank->name() : "Unknown bank";
      constexpr size_t max_len = 16;
      if (label.size() > max_len) {
        label.resize(max_len);
        label[max_len - 1] = '.';
      }
      context_.font->render_text_onto(
          *ctx.surface, label, {col_city, y}, FontFace::PIZ_NUMB,
          TextStyle{.color = 240, .character_spacing = 1});
    }
    else {
      context_.font->render_text_onto(*ctx.surface, core::city_name(city_id),
                                      {col_city, y}, FontFace::PIZ_NUMB, 240);
    }

    context_.font->render_text_onto(
        *ctx.surface, compat::format("{}", loan->start_amount()),
        {col_amount, y}, FontFace::PIZ_NUMB, 240, TextAnchor::End);
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{}", loan->months_left()), {col_time, y},
        FontFace::PIZ_NUMB, 240, TextAnchor::End);
    context_.font->render_text_onto(
        *ctx.surface, std::to_string(loan->interest_percentage()), {col_pct, y},
        FontFace::PIZ_NUMB, 240, TextAnchor::End);
    context_.font->render_text_onto(*ctx.surface, " %", {col_pct + 10, y},
                                    FontFace::PIZ_NUMB, 240);
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{}", loan->monthly_repayment()),
        {col_month, y}, FontFace::PIZ_NUMB, 240, TextAnchor::End);
  }

  constexpr int sep_y = 168;

  context_.font->render_text_onto(*ctx.surface, "investments", {12, 168},
                                  FontFace::ASTANDAR);

  const auto& investments = player->investments();

  constexpr int inv_row_y0 = 224;
  constexpr int inv_row_height = 16;
  for (int slot = 0; slot < Player::max_investment_count; slot++) {
    int y = inv_row_y0 + (slot * inv_row_height);
    const auto& inv = investments[slot];
    if (!inv.has_value() || inv->is_matured()) {
      continue;
    }

    if (inv->city_id() == active_city_id) {
      const auto* city = state_.city(inv->city_id());
      const Actor* bank =
          city ? city->actor_by_id(static_cast<uint8_t>(inv->bank_id().value))
               : nullptr;
      std::string label = bank ? bank->name() : "Unknown bank";
      context_.font->render_text_onto(*ctx.surface, label, {col_city, y},
                                      FontFace::PIZ_NUMB, 240);
    }
    else {
      context_.font->render_text_onto(*ctx.surface,
                                      core::city_name(inv->city_id()),
                                      {col_city, y}, FontFace::PIZ_NUMB, 240);
    }

    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:i}", inv->amount()), {col_amount, y},
        FontFace::PIZ_NUMB, 240, TextAnchor::End);
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{}", inv->months_left()), {col_time, y},
        FontFace::PIZ_NUMB, 240, TextAnchor::End);
    context_.font->render_text_onto(*ctx.surface,
                                    compat::format("{}%", inv->return_rate()),
                                    {col_pct, y}, FontFace::PIZ_NUMB, 240);
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{}", inv->monthly_return()),
        {col_month, y}, FontFace::PIZ_NUMB, 240, TextAnchor::End);
  }
}

void GlobalStats::render_insurance(const RenderContext& ctx) {
  render_background(ctx, false);

  constexpr ViewPoint title_pos{136, 50};
  constexpr ViewPoint header_category_pos{18, 100};
  constexpr ViewPoint header_company_pos{126, 100};
  constexpr ViewPoint header_charge_pos{296, 92};
  constexpr ViewPoint header_cover_pos{296, 110};

  constexpr int col_category_x = 18;
  constexpr int col_company_x = 120;
  constexpr int col_value_x = 296;

  constexpr int row_y0 = 132;
  constexpr int row_height = 40;
  constexpr int cover_y_off = 18;

  constexpr std::array<const char*, 5> categories{"burglary", "warehouse",
                                                  "fire", "liability", "life"};

  context_.font->render_text_onto(*ctx.surface, "Insurance", title_pos,
                                  FontFace::RUBY);

  context_.font->render_text_onto(*ctx.surface, "category", header_category_pos,
                                  FontFace::PIZ_NUMB, 13);
  context_.font->render_text_onto(*ctx.surface, "company", header_company_pos,
                                  FontFace::PIZ_NUMB, 13);
  context_.font->render_text_onto(*ctx.surface, "charge", header_charge_pos,
                                  FontFace::PIZ_NUMB, 13);
  context_.font->render_text_onto(*ctx.surface, "cover", header_cover_pos,
                                  FontFace::PIZ_NUMB, 13);

  auto* player = state_.active_player();
  const auto& policies = player->insurance_state().policies;
  for (int i = 0; i < static_cast<int>(categories.size()); ++i) {
    const int y = row_y0 + (i * row_height);
    const ViewPoint category_pos{col_category_x, y};
    const ViewPoint company_pos{col_company_x, y};
    const ViewPoint premium_pos{col_value_x, y};
    const ViewPoint cover_pos{col_value_x, y + cover_y_off};
    context_.font->render_text_onto(*ctx.surface,
                                    std::string(categories[i]) + ":",
                                    category_pos, FontFace::PIZ_NUMB, 240);

    const auto& policy = policies[i];

    if (policy.is_active()) {
      context_.font->render_text_onto(*ctx.surface, policy.company_name,
                                      company_pos, FontFace::PIZ_NUMB, 13);
      context_.font->render_text_onto(
          *ctx.surface, compat::format("{}", policy.monthly_premium),
          premium_pos, FontFace::PIZ_NUMB, 240);
      context_.font->render_text_onto(*ctx.surface,
                                      compat::format("{}", policy.max_coverage),
                                      cover_pos, FontFace::PIZ_NUMB, 240);
    }
    else {
      context_.font->render_text_onto(*ctx.surface, "---------------",
                                      company_pos, FontFace::PIZ_NUMB, 13);
    }
  }
}

void GlobalStats::render_property(const RenderContext& ctx) {
  render_background(ctx, false);

  context_.font->render_text_onto(*ctx.surface, "finance info", {56, 30},
                                  FontFace::RUBY);
  context_.font->render_text_onto(*ctx.surface, "property", {22, 60},
                                  FontFace::ASTANDAR);

  auto* player = state_.active_player();
  auto player_id = player->id();
  auto cities = state_.visited_cities(*player);

  struct PropEntry {
    Property* prop;
    const City* city;
  };
  std::vector<PropEntry> owned;
  std::vector<PropEntry> rented;
  for (auto* city : cities) {
    for (auto* prop : city->properties()) {
      auto tenant = prop->owner_or_tenant();
      if (!tenant.has_value() || *tenant != player_id) {
        continue;
      }
      if (prop->is_owned()) {
        owned.push_back({prop, city});
      }
      else if (prop->is_rented()) {
        rented.push_back({prop, city});
      }
    }
  }

  auto venue_name = [&](Property* prop) -> std::string {
    if (prop->type() == PropertyType::DEPOT) {
      return "warehouse";
    }
    if (prop->has_restaurant()) {
      auto* r = state_.restaurant(*prop->restaurant_id());
      return r ? r->name() : "restaurant";
    }
    return "empty";
  };

  constexpr int header_x = 14;
  constexpr int data_x = 4;
  constexpr int venue_x = 152;
  constexpr int entry_y0 = 120;
  constexpr int row_height = 18;
  constexpr int cutoff_y = 344;
  constexpr int rented_header_cutoff = 332;

  context_.font->render_text_onto(*ctx.surface, "property:", {header_x, 78},
                                  FontFace::PIZ_NUMB);

  int y = entry_y0;
  for (auto& e : owned) {
    if (y > cutoff_y) {
      break;
    }
    context_.font->render_text_onto(*ctx.surface, e.prop->district().name(),
                                    {data_x, y}, FontFace::PIZ_NUMB, 240);
    context_.font->render_text_onto(*ctx.surface, venue_name(e.prop),
                                    {venue_x, y}, FontFace::PIZ_NUMB, 240);
    y += row_height;
  }
  y += row_height;

  if (y < rented_header_cutoff) {
    context_.font->render_text_onto(*ctx.surface, "rented:", {header_x, y - 4},
                                    FontFace::PIZ_NUMB);
    y += row_height;
    for (auto& e : rented) {
      if (y > cutoff_y) {
        break;
      }
      context_.font->render_text_onto(*ctx.surface, e.prop->district().name(),
                                      {data_x, y}, FontFace::PIZ_NUMB, 240);
      context_.font->render_text_onto(*ctx.surface, venue_name(e.prop),
                                      {venue_x, y}, FontFace::PIZ_NUMB, 240);
      y += row_height;
    }
  }
}

void GlobalStats::render_balance(const RenderContext& ctx) {
  render_background(ctx, true);

  TextStyle ts{
      .color = 240,
      .align = TextAnchor::Start,
      .shadow =
          ShadowStyle{
              .offset = {-2, 2},
              .color = 13,
          },
      .character_spacing = std::nullopt,
  };

  context_.font->render_text_onto(*ctx.surface, "Balance", {136, 42},
                                  FontFace::RUBY, ts);
  context_.font->render_text_onto(*ctx.surface, "Assets", {12, 82},
                                  FontFace::RUBY, ts);
  context_.font->render_text_onto(*ctx.surface, "Liabilities", {214, 82},
                                  FontFace::RUBY, ts);

  render_balance_lines(ctx);

  constexpr int line_spacing = 22;
  constexpr uint8_t item_color = 240;
  // The sheet has an assets and a liabilities side; each one's amounts end
  // this far left of the panel edge, measured from that side's label x.
  constexpr int amount_inset = 210;
  auto amount_x = [&ctx](ViewPoint label_pos) {
    return ctx.surface->width() + label_pos.x - amount_inset;
  };

  auto draw_label_and_amount = [&](std::string_view label, Money amount,
                                   ViewPoint pos) {
    // account name
    context_.font->render_text_onto(*ctx.surface, compat::format("{}", label),
                                    pos, FontFace::PIZ_NUMB, item_color);

    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:i}", amount),
        ViewPoint{amount_x(pos), pos.y}, FontFace::PIZ_NUMB, item_color,
        TextAnchor::End);
  };
  auto balance =
      BalanceSheet(*state_.active_player(), state_.options().fix_legacy_bugs);
  // Assets
  ViewPoint label_pos{6, 120};
  Money loss_amount = 0_usd;
  bool has_loss = false;
  for (const auto& entry : balance.assets()) {
    if (entry.account == Account::ProfitAndLoss) {
      has_loss = true;
      loss_amount = entry.amount;
      continue;
    }
    std::string label = (entry.account == Account::Stock)
                            ? "supplies"
                            : to_string(entry.account);
    draw_label_and_amount(label, entry.amount, label_pos);
    label_pos.y += line_spacing;
  }
  // loss sits one extra line below the regular assets, drawn in red
  if (has_loss) {
    const int text_color = 22;
    label_pos.y = 276;
    context_.font->render_text_onto(*ctx.surface, "loss", label_pos,
                                    FontFace::PIZ_NUMB, text_color);
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{:i}", loss_amount),
        ViewPoint{amount_x(label_pos), label_pos.y}, FontFace::PIZ_NUMB,
        text_color, TextAnchor::End);
  }
  constexpr int total_y = 276 + 36;
  draw_label_and_amount("", balance.total_assets(), ViewPoint{6, total_y});

  label_pos = ViewPoint{190, 120};
  for (const auto& entry : balance.liabilities()) {
    // ProfitAndLoss shows as "profit" on liability side when positive
    std::string label = (entry.account == Account::ProfitAndLoss)
                            ? "profit"
                            : to_string(entry.account);
    // Amounts are already negated in BalanceSheet to show as positive
    draw_label_and_amount(label, entry.amount, label_pos);
    label_pos.y += line_spacing;
  }
  draw_label_and_amount("", balance.total_liabilities(),
                        ViewPoint{190, total_y});
}

void GlobalStats::render_balance_lines(const RenderContext& ctx) {
  int line_color = 240;
  int line_width = 2;
  // Main vertical line
  gfx::draw::line(*ctx.surface, {184, 90}, {184, 340}, line_color, line_width);
  // Main horizontal line
  gfx::draw::line(*ctx.surface, {6, 112}, {376, 112}, line_color, line_width);
  // Horizontal line under asset labels
  gfx::draw::line(*ctx.surface, {4, 306}, {74, 306}, line_color, line_width);
  // Horizontal line under asset amounts
  gfx::draw::line(*ctx.surface, {100, 306}, {180, 306}, line_color, line_width);
  // Horizontal line under liabilities labels
  gfx::draw::line(*ctx.surface, {188, 306}, {250, 306}, line_color, line_width);
  // Diagonal line towards liabilities amounts
  gfx::draw::line(*ctx.surface, {250, 306}, {308, 246}, line_color, line_width);
  gfx::draw::line(*ctx.surface, {308, 246}, {382, 246}, line_color, line_width);

  gfx::draw::line(*ctx.surface, {312, 306}, {380, 306}, line_color, line_width);
  // Final horizontal bars across whole screen
  gfx::draw::line(*ctx.surface, {6, 330}, {376, 330}, line_color, line_width);
  gfx::draw::line(*ctx.surface, {6, 334}, {376, 334}, line_color, line_width);
}
