/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <vector>
#include "views/base/RestaurantSubView.h"
#include "views/ui/status/StatusController.h"

class Contracts : public RestaurantSubView {
public:
  Contracts(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;
  void navigate_to_restaurant(core::RestaurantId new_id) override;

private:
  void set_up_input_panel();
  void flip_page();
  void render_front(const RenderContext& ctx);
  void render_back(const RenderContext& ctx);
  void cancel_current();
  void update_contract_list();
  std::vector<size_t>
      contract_indices_; // indices into player->contracts() for this restaurant

  int current_pos_{0}; // position in contract_indices_
  bool show_back_{false};

  StatusController status_controller_;
};
