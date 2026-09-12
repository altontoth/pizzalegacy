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

  bool buy_restaurant(Property& property, Player& buyer, Money price,
                      bool free = false);
  Restaurant* rent_restaurant(Property& property, Player& player,
                              bool free = false);
  bool buy_depot(Property& property, Player& buyer, Money price);
  bool rent_depot(Property& property, Player& player, bool free = false);
  std::vector<Property*> depots_for(core::PlayerId pid) const;
  bool sell_property(Property& property, Player& seller, Money sale_price);
  bool end_property_lease(Property& property, Player& player);

  Restaurant* create_restaurant(Player& player, Property& property);
  // Create a restaurant from one of the quick-start layout files, in a random
  // available property.
  Restaurant* create_restaurant_with_layout(Player& player, City& city,
                                            int layout_id);
  // How a new restaurant gets its premise.
  enum class Tenure { RentFirstMonthFree, Rent, Buy };

  // Furnishes from one of the prebuilt AI restaurants, which carry their own
  // size, and shows only its opening furniture.
  Restaurant* create_ai_restaurant(Player& player, Property& property,
                                   Tenure tenure);
  Restaurant* create_ai_starting_restaurant(Player& player, City& city);
  Property* find_available_premise(City& city);

private:
  Restaurant* furnish_new_restaurant(Player& player, Property& property,
                                     std::unique_ptr<RestaurantLayout> layout,
                                     Tenure tenure);
  void close_restaurant(Player& player, Property& property);

  GameState* gs_;
};
