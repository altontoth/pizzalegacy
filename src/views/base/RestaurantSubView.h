/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "views/base/MainPanelView.h"

// Base for restaurant sub-screens (stats, staff, stock, purchasing, etc.) so
// that we get the next/previous restaurant for free.
class RestaurantSubView : public MainPanelView, public input::InputListener {
public:
  RestaurantSubView(GameContext& context, GameState& state,
                    core::RestaurantId restaurant_id);
  bool on_action(const input::ActionEvent& ev) override;

protected:
  virtual void navigate_to_restaurant(core::RestaurantId new_id) = 0;

  core::RestaurantId restaurant_id_;
  input::ListenerToken listener_;
};
