/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <utility>
#include <vector>
#include "core/common/Ids.h"
#include "core/restaurant/RestaurantStats.h"

class Restaurant;
class GameDate;

namespace core::restaurant {

using PizzaSalesVec =
    std::vector<std::pair<core::RecipeId, core::restaurant::PizzaSalesData>>;

// Returns weekly pizza sales for only the pizzas currently on the menu,
// in menu insertion order. Pizzas with no sales this week are included
// with zero counts. Behavior differs slightly from the original; when you
// remove a piza and then re-add it the original sets all the values to zero.
// But we have better memory so we just restore the data because it's the same
// pizza.
PizzaSalesVec aggregate_weekly_pizza_sales(const Restaurant& restaurant,
                                           const GameDate& date);

} // namespace core::restaurant
