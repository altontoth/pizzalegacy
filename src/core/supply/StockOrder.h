/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <vector>
#include "core/finance/Money.h"
#include "core/food/IngredientType.h"
#include "core/supply/IngredientBatch.h"

class City;
class FoodSupplier;
class GameState;
class Player;
class Restaurant;

struct ShippedIngredient {
  core::food::IngredientType type;
  int kg{0};
};

// An order's price at the quoted prices.
Money order_total(const std::vector<IngredientBatch>& order);

// False when the player can't pay (nothing gets bought).
bool buy_for_restaurant(GameState& state, Player& player, City& city,
                        Restaurant& restaurant, FoodSupplier* supplier,
                        const std::vector<IngredientBatch>& order);
bool buy_for_depot(GameState& state, Player& player, City& city,
                   FoodSupplier* supplier,
                   const std::vector<IngredientBatch>& order);

void sell_from_depot(GameState& state, Player& player, City& city,
                     const std::vector<IngredientBatch>& order);

// Moves stock between the player's depots in two cities. False when the
// player can't pay the transport cost (nothing moves).
bool ship_between_depots(GameState& state, Player& player, City& from, City& to,
                         const std::vector<ShippedIngredient>& items,
                         Money transport_cost);
