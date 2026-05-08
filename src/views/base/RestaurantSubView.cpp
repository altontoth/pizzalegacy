/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/base/RestaurantSubView.h"

#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "engine/input/InputManager.h"
#include "views/ui/status/StatusController.h"

RestaurantSubView::RestaurantSubView(GameContext& context, GameState& state,
                                     core::RestaurantId restaurant_id)
    : MainPanelView(context, state), restaurant_id_(restaurant_id),
      listener_(context_.input->register_listener(*this)) {}

bool RestaurantSubView::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }

  auto* player = state_.active_player();
  if (!player) {
    return false;
  }

  const auto current_id = restaurant_id_;

  auto navigate = [&](core::RestaurantId new_id) {
    navigate_to_restaurant(new_id);
    // Immediately update the status bar so it reflects the new restaurant in
    // the same frame. Screens with their own StatusController will re-set it
    // in their constructor next frame; screens without one rely on this call.
    StatusController sc(state_.status());
    sc.show_restaurant({.state = &state_,
                        .rest = state_.active_restaurant(),
                        .label = "RESTAURANT"});
  };

  switch (ev.action) {
  case input::Action::NextRestaurant:
    if (auto next = state_.next_restaurant(player->id(), current_id)) {
      if (*next != current_id) {
        navigate(*next);
      }
    }
    return true;
  case input::Action::PreviousRestaurant:
    if (auto prev = state_.prev_restaurant(player->id(), current_id)) {
      if (*prev != current_id) {
        navigate(*prev);
      }
    }
    return true;
  default:
    return false;
  }
}
