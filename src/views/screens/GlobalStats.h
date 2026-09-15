/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/finance/Ledger.h"
#include "resources/Blitmap.h"
#include "resources/Bob.h"
#include "views/base/MainPanelView.h"

#include <optional>

class Restaurant;

enum class GlobalStatsScreen {
  Month_Overview,
  Restaurants,
  Cash_Flow,
  Loans,
  Insurance,
  Property,
  Balance,
  Daily_Profits,
  None,
};

struct MonthlyPopupState {
  bool visible{false};
  Account account{Account::Sales};
  bool is_receipts{true};
  bool show_category_total{false};
  int scroll_offset{0};
  static constexpr int max_visible{11};
};

class GlobalStats : public MainPanelView {
public:
  GlobalStats(GameContext& context, GameState& state,
              std::optional<GlobalStatsScreen> screen = std::nullopt);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  void create_buttons();
  void render_month_overview(const RenderContext& ctx);
  void render_month_overview_popup(const RenderContext& ctx,
                                   const std::vector<Restaurant*>& restaurants);
  void render_cash_flow(const RenderContext& ctx);
  void render_loans(const RenderContext& ctx);
  void render_insurance(const RenderContext& ctx);
  void render_property(const RenderContext& ctx);
  void render_balance(const RenderContext& ctx);
  void render_daily_profits(const RenderContext& ctx);
  void render_bar_chart(const RenderContext& ctx,
                        const std::vector<int64_t>& data,
                        std::string_view title, std::string_view x_axis_label);
  void render_background(const RenderContext& ctx, bool grid);
  void render_binder(const RenderContext& ctx);
  void render_balance_lines(const RenderContext& ctx);

  GlobalStatsScreen current_screen_{GlobalStatsScreen::Month_Overview};
  Bob& stanbobs_;
  Blitmap<Blitmap2Icon>& blitmap2_;
  MonthlyPopupState popup_state_;
  int property_scroll_offset_{0};
};
