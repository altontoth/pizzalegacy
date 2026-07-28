/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <vector>

#include "core/common/Ids.h"
#include "core/finance/Money.h"

class City;
class GameState;
class Player;
class Property;
class Restaurant;
class RestaurantLayout;

// Property transactions and the restaurant creation they trigger.
class RealEstate {
public:
  explicit RealEstate(GameState& state) : gs_(&state) {}

  bool buy_restaurant(Property& property, Player& buyer, Money price);
  Restaurant* rent_restaurant(Property& property, Player& player,
                              bool free = false);
  bool buy_depot(Property& property, Player& buyer);
  bool rent_depot(Property& property, Player& player, bool free = false);
  std::vector<Property*> depots_for(core::PlayerId pid) const;
  bool sell_property(Property& property, Player& seller, Money sale_price);
  bool end_property_lease(Property& property, Player& player);

  Restaurant* create_restaurant(Player& player, Property& property);
  // Create a restaurant with a specific layout in a random available property.
  // Pass `layout` to furnish it from an existing RestaurantLayout (we'll ignore
  // layout_id in that case).
  Restaurant*
  create_restaurant_with_layout(Player& player, City& city, int layout_id,
                                bool is_big,
                                std::unique_ptr<RestaurantLayout> layout = {});
  Restaurant* create_ai_starting_restaurant(Player& player, City& city);

private:
  GameState* gs_;
};
