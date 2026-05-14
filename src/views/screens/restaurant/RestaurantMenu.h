/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>

#include "core/common/Ids.h"
#include "core/restaurant/Restaurant.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "engine/input/TextSession.h"
#include "views/base/MainPanelView.h"

enum class MenuViewState {
  MENU,             // Menu list with prices
  SALES,            // Pizza sales statistics
  CUSTOMER_RATINGS, // Customer demographic ratings
};

class RestaurantMenu : public MainPanelView,
                       public input::InputListener,
                       public debug::AutoLabel<RestaurantMenu> {
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

  void begin_price_edit(int row);
  void end_price_edit();
  void commit_price_edit();

  bool on_text(const input::TextEvent& ev) override;
  bool on_action(const input::ActionEvent& ev) override;

  static constexpr int max_price_input_length = 5;

  core::RestaurantId id_;
  Restaurant* restaurant_;
  MenuViewState current_state_ = MenuViewState::MENU;
  int current_page_ = 0;
  std::optional<int> selected_row_;
  std::optional<int> editable_price_row_;
  std::string new_price_;
  input::ListenerToken input_;
  std::optional<input::TextSession> text_session_;
};
