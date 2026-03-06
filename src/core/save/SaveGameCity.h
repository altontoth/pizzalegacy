/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <nlohmann/json.hpp>
#include "core/city/City.h"
#include "core/city/ControlMap.h"
#include "core/city/EconomyTrend.h"
#include "core/city/PizzaCharts.h"

class GameState;
class GameContext;
class FoodSupplier;

namespace save {

using json = nlohmann::json;

json property_to_json(const Property& property);
void json_to_property(const json& j, Property& property);

json supplier_to_json(const FoodSupplier& supplier);
void json_to_supplier(const json& j, FoodSupplier& supplier);

json pizza_charts_to_json(const PizzaCharts& charts);
void json_to_pizza_charts(const json& j, PizzaCharts& charts);

json control_map_to_json(const ControlMap& cm);
void json_to_control_map(const json& j, ControlMap& cm);

json economy_trend_to_json(const EconomyTrend& et);
void json_to_economy_trend(const json& j, EconomyTrend& et);

json city_to_json(const City& city);
void json_to_city(const json& j, City& city);

} // namespace save
