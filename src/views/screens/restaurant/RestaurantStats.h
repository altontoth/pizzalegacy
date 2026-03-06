/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include "core/common/Ids.h"
#include "resources/Blitmap.h"
#include "resources/Bob.h"
#include "views/base/MainPanelView.h"

class Restaurant;

enum class RestaurantStatsScreen {
  Daily_Balance,
  Week_Review,
  Month_Review,
  Weeks_Takings,
  Customer_Stats,
  Sales_Curve,
  None,
};

class RestaurantStats : public MainPanelView {
public:
  RestaurantStats(GameContext& context, GameState& state,
                  core::RestaurantId restaurant_id,
                  std::optional<RestaurantStatsScreen> screen = std::nullopt,
                  bool automatic_mode = false);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  void create_buttons();
  void render_daily_balance(const RenderContext& ctx);
  void render_week_review(const RenderContext& ctx);
  void render_month_review(const RenderContext& ctx);
  void render_customer_stats(const RenderContext& ctx);
  void render_sales_curve(const RenderContext& ctx);
  void render_background(const RenderContext& ctx, bool grid);
  void render_binder(const RenderContext& ctx);
  void render_paper(const RenderContext& ctx, ViewRect box);

  core::RestaurantId restaurant_id_;
  Restaurant* restaurant_{nullptr};
  RestaurantStatsScreen current_screen_{RestaurantStatsScreen::Daily_Balance};
  bool automatic_mode_{false};
  Bob& stanbobs_;
  Blitmap<Blitmap2Icon>& blitmap2_;
};
