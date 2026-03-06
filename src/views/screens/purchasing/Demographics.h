/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "core/restaurant/Restaurant.h"
#include "views/base/MainPanelView.h"

namespace views::screens::purchasing {

class Demographics : public MainPanelView {
public:
  Demographics(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  void set_up_buttons();

  core::RestaurantId id_;
  Restaurant* restaurant_;
};

} // namespace views::screens::purchasing
