/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/restaurant/Restaurant.h"
#include "views/base/RestaurantSubView.h"

namespace views::screens::purchasing {

class Demographics : public RestaurantSubView {
public:
  Demographics(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;
  void navigate_to_restaurant(core::RestaurantId new_id) override;

private:
  void set_up_buttons();

  Restaurant* restaurant_;
};

} // namespace views::screens::purchasing
