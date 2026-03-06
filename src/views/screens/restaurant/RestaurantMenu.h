/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "core/restaurant/Restaurant.h"
#include "views/base/MainPanelView.h"

enum class MenuViewState {
  MENU,             // Menu list with prices
  SALES,            // Pizza sales statistics
  CUSTOMER_RATINGS, // Customer demographic ratings
};

class RestaurantMenu : public MainPanelView {
public:
  RestaurantMenu(GameContext& context, GameState& state,
                 core::RestaurantId r_id);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  void set_up_buttons();
  void render_menu_view(const RenderContext& ctx);
  void render_sales_view(const RenderContext& ctx);
  void render_customer_ratings_view(const RenderContext& ctx);

  core::RestaurantId id_;
  Restaurant* restaurant_;
  MenuViewState current_state_ = MenuViewState::MENU;
  int current_page_ = 0;
  std::optional<int> selected_row_;
};
