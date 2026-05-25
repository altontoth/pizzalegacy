/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <nlohmann/json.hpp>
#include "core/character/Staff.h"
#include "core/restaurant/FloorPattern.h"
#include "core/restaurant/Menu.h"
#include "core/restaurant/PlacedFurniture.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/RestaurantLayout.h"

namespace save {

using json = nlohmann::json;

// Attorney for private Restaurant access
struct RestaurantLoader {
  static void set_wants_open(Restaurant& r, bool v) { r.wants_open(v); }
};

json staff_to_json(const Staff& staff);
Staff json_to_staff(const json& j);

json menu_to_json(const core::restaurant::Menu& menu);
core::restaurant::Menu json_to_menu(const json& j);

json floor_pattern_to_json(const FloorPattern& pattern);
FloorPattern json_to_floor_pattern(const json& j);

json placed_furniture_to_json(const PlacedFurniture& pf);
std::unique_ptr<PlacedFurniture> json_to_placed_furniture(const json& j);

json restaurant_layout_to_json(const RestaurantLayout& layout);
std::unique_ptr<RestaurantLayout> json_to_restaurant_layout(const json& j);

json restaurant_to_json(const Restaurant& restaurant);
std::unique_ptr<Restaurant> json_to_restaurant(const json& j,
                                               Property& property);

// RestaurantStats serialization
json pizza_sales_data_to_json(const core::restaurant::PizzaSalesData& data);
core::restaurant::PizzaSalesData json_to_pizza_sales_data(const json& j);

json daily_stats_to_json(const core::restaurant::DailyStats& stats);
core::restaurant::DailyStats json_to_daily_stats(const json& j);

json restaurant_stats_to_json(const core::restaurant::RestaurantStats& stats);
core::restaurant::RestaurantStats json_to_restaurant_stats(const json& j);

// RestaurantInventory serialization
json ingredient_stock_to_json(const core::restaurant::IngredientStock& stock);
core::restaurant::IngredientStock json_to_ingredient_stock(const json& j);

json restaurant_inventory_to_json(
    const core::restaurant::RestaurantInventory& inventory);
core::restaurant::RestaurantInventory
json_to_restaurant_inventory(const json& j);

} // namespace save
