/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/save/SaveGameRestaurant.h"

#include <ctime>

#include "core/character/Staff.h"
#include "core/city/City.h"
#include "core/city/Property.h"
#include "core/logistics/IngredientStock.h"
#include "core/restaurant/FloorPattern.h"
#include "core/restaurant/Menu.h"
#include "core/restaurant/PlacedFurniture.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/RestaurantLayout.h"
#include "core/save/SaveGameCharacter.h"
#include "engine/Logger.h"
#include "engine/geom/Spaces.h"
#include "factories/FurnitureFactory.h"

namespace save {

json staff_to_json(const Staff& staff) {
  json j;
  j["job_role"] = static_cast<int>(staff.job_role());
  j["character"] = character_to_json(staff.character());
  j["payment_cents"] = staff.payment().cents;
  // TODO: current_training serialization if needed
  return j;
}

Staff json_to_staff(const json& j) {
  auto job_role =
      static_cast<core::character::JobRole>(j["job_role"].get<int>());
  auto character = json_to_character(j["character"]);
  Staff staff(job_role, character);

  // Handle both old format (dollars as int) and new format (cents as int64)
  if (j.contains("payment_cents")) {
    staff.payment(Money{j["payment_cents"].get<int64_t>()});
  }
  else if (j.contains("payment")) {
    // Legacy format: payment in dollars, convert to cents
    int dollars = j["payment"].get<int>();
    staff.payment(Money{dollars * 100});
  }

  // TODO: current_training deserialization if needed
  return staff;
}

json menu_to_json(const core::restaurant::Menu& menu) {
  json j = json::array();
  for (const auto& entry : menu.entries()) {
    json entry_json;
    entry_json["recipe_id"] = entry.recipe_id.value;
    entry_json["price"] = entry.price.cents;

    // Save popularity multipliers
    json multipliers = json::array();
    for (const auto& mult : entry.popularity_multiplier) {
      multipliers.push_back(mult);
    }
    entry_json["popularity_multiplier"] = multipliers;

    j.push_back(entry_json);
  }
  return j;
}

core::restaurant::Menu json_to_menu(const json& j) {
  core::restaurant::Menu menu;
  for (const auto& entry_json : j) {
    core::RecipeId recipe_id{entry_json["recipe_id"].get<int>()};
    Money price{entry_json["price"].get<int64_t>()};

    int idx = menu.add_or_set(recipe_id, price);

    // Restore popularity multipliers
    if (entry_json.contains("popularity_multiplier")) {
      auto& entry = menu.entries()[idx];
      for (size_t i = 0; i < entry_json["popularity_multiplier"].size(); ++i) {
        entry.popularity_multiplier[i] =
            entry_json["popularity_multiplier"][i].get<float>();
      }
    }
  }
  return menu;
}

json floor_pattern_to_json(const FloorPattern& pattern) {
  json j;
  j["vendor_id"] = pattern.vendor_id;
  j["tile_indices"] = pattern.tile_indices;
  j["popularity"] = pattern.popularity;
  j["popularity_factor"] = pattern.popularity_factor;
  return j;
}

FloorPattern json_to_floor_pattern(const json& j) {
  FloorPattern pattern;
  pattern.vendor_id = j["vendor_id"].get<int>();

  for (size_t i = 0; i < 4 && i < j["tile_indices"].size(); ++i) {
    pattern.tile_indices[i] = j["tile_indices"][i].get<int>();
  }

  for (size_t i = 0;
       i < pattern.popularity.size() && i < j["popularity"].size(); ++i) {
    pattern.popularity[i] = j["popularity"][i].get<int>();
  }

  pattern.popularity_factor = j["popularity_factor"].get<int>();
  return pattern;
}

json placed_furniture_to_json(const PlacedFurniture& pf) {
  json j;

  // Get the FurnitureType enum value
  auto furniture_type_opt =
      FurnitureFactory::get_furniture_type(pf.furniture());
  if (!furniture_type_opt.has_value()) {
    Logger::warn("Failed to find FurnitureType for furniture, skipping");
    return json::object();
  }

  j["furniture_type"] = static_cast<int>(*furniture_type_opt);
  j["id"] = pf.id().value;
  j["origin"] = {{"x", pf.origin().x}, {"y", pf.origin().y}};
  j["bob_state"] = pf.bob_state();

  if (pf.linked_furniture_id().has_value()) {
    j["linked_furniture_id"] = pf.linked_furniture_id()->value;
  }
  else {
    j["linked_furniture_id"] = nullptr;
  }

  return j;
}

std::unique_ptr<PlacedFurniture> json_to_placed_furniture(const json& j) {
  int furniture_type = j["furniture_type"].get<int>();
  const Furniture* furniture = FurnitureFactory::create_furniture(
      static_cast<FurnitureType>(furniture_type));

  if (!furniture) {
    Logger::warn("Failed to find furniture type {}", furniture_type);
    return nullptr;
  }

  core::FurnitureId id{j["id"].get<int>()};
  WorldPoint origin{j["origin"]["x"].get<int>(), j["origin"]["y"].get<int>()};
  int bob_state = j["bob_state"].get<int>();

  core::FurnitureId linked_id{UNLINKED_FURNITURE};
  if (j.contains("linked_furniture_id") &&
      !j["linked_furniture_id"].is_null()) {
    linked_id = core::FurnitureId{j["linked_furniture_id"].get<int>()};
  }

  return std::make_unique<PlacedFurniture>(furniture, id, origin, linked_id,
                                           bob_state);
}

json restaurant_layout_to_json(const RestaurantLayout& layout) {
  json j;

  j["restaurant_size"] = {{"w", layout.restaurant_size().w},
                          {"h", layout.restaurant_size().h}};
  j["kitchen_size"] = {{"w", layout.kitchen_size().w},
                       {"h", layout.kitchen_size().h}};
  j["entrance_offset"] = layout.entrance_offset();
  j["kitchen_entry_offset"] = layout.kitchen_entry_offset();

  // Save floor pattern
  j["floor_pattern"] = floor_pattern_to_json(layout.floor_pattern());

  // Save placed furniture
  json furniture_array = json::array();
  for (const auto& pf : layout.placed_furniture()) {
    auto pf_json = placed_furniture_to_json(*pf);
    if (!pf_json.empty()) { // Skip furniture that failed type lookup
      furniture_array.push_back(pf_json);
    }
  }
  j["placed_furniture"] = furniture_array;

  // Save kitchen furniture
  json kitchen_furniture_array = json::array();
  for (const auto& pf : layout.placed_kitchen_furniture()) {
    auto pf_json = placed_furniture_to_json(*pf);
    if (!pf_json.empty()) { // Skip furniture that failed type lookup
      kitchen_furniture_array.push_back(pf_json);
    }
  }
  j["placed_kitchen_furniture"] = kitchen_furniture_array;

  return j;
}

std::unique_ptr<RestaurantLayout> json_to_restaurant_layout(const json& j) {
  Size restaurant_size{j["restaurant_size"]["w"].get<int>(),
                       j["restaurant_size"]["h"].get<int>()};
  Size kitchen_size{j["kitchen_size"]["w"].get<int>(),
                    j["kitchen_size"]["h"].get<int>()};
  int entrance_offset = j["entrance_offset"].get<int>();
  int kitchen_entry_offset = j["kitchen_entry_offset"].get<int>();

  auto layout = std::make_unique<RestaurantLayout>(
      restaurant_size, kitchen_size, entrance_offset, kitchen_entry_offset);

  // Restore floor pattern
  if (j.contains("floor_pattern")) {
    layout->floor_pattern(json_to_floor_pattern(j["floor_pattern"]));
  }

  // Restore placed furniture
  if (j.contains("placed_furniture")) {
    for (const auto& pf_json : j["placed_furniture"]) {
      auto pf = json_to_placed_furniture(pf_json);
      if (pf) {
        layout->place_furniture(std::move(pf));
      }
    }
  }

  // Restore kitchen furniture
  if (j.contains("placed_kitchen_furniture")) {
    for (const auto& pf_json : j["placed_kitchen_furniture"]) {
      auto pf = json_to_placed_furniture(pf_json);
      if (pf) {
        layout->place_kitchen_furniture(std::move(pf));
      }
    }
  }

  return layout;
}

json restaurant_to_json(const Restaurant& restaurant) {
  json j;
  j["id"] = restaurant.id().value;
  j["name"] = restaurant.name();
  j["wants_open"] = restaurant.wants_open();
  j["forced_days_remaining"] = restaurant.forced_days_remaining();
  j["kitchen_size"] = {{"w", restaurant.kitchen_size().w},
                       {"h", restaurant.kitchen_size().h}};

  // Save property reference (city ID + property index)
  if (restaurant.property()) {
    j["property_city_id"] = restaurant.property()->city()->id().value;
    j["property_index"] = restaurant.property()->index();
  }

  // Save menu
  j["menu"] = menu_to_json(restaurant.menu());

  // Save staff arrays
  json cooks_array = json::array();
  for (const auto& cook_opt : restaurant.cooks()) {
    if (cook_opt.has_value()) {
      cooks_array.push_back(staff_to_json(*cook_opt));
    }
    else {
      cooks_array.push_back(nullptr);
    }
  }
  j["cooks"] = cooks_array;

  json waiters_array = json::array();
  for (const auto& waiter_opt : restaurant.waiters()) {
    if (waiter_opt.has_value()) {
      waiters_array.push_back(staff_to_json(*waiter_opt));
    }
    else {
      waiters_array.push_back(nullptr);
    }
  }
  j["waiters"] = waiters_array;

  // Save manager
  if (restaurant.manager().has_value()) {
    j["manager"] = staff_to_json(*restaurant.manager());
  }
  else {
    j["manager"] = nullptr;
  }

  // Save layout
  if (restaurant.layout()) {
    j["layout"] = restaurant_layout_to_json(*restaurant.layout());
  }
  else {
    j["layout"] = nullptr;
  }

  // Save stats
  j["stats"] = restaurant_stats_to_json(restaurant.stats());

  // Save operating hours
  j["opening_time"] = {{"hours", restaurant.opening_time().hour},
                       {"minutes", restaurant.opening_time().minute}};
  j["closing_time"] = {{"hours", restaurant.closing_time().hour},
                       {"minutes", restaurant.closing_time().minute}};

  // Save full economy state
  j["total_popularity"] = restaurant.economy().total_popularity();
  j["quality_rating"] = restaurant.economy().quality_rating();
  j["rejection_rate"] = restaurant.economy().rejection_rate();
  j["range_raw"] = restaurant.economy().range_raw();
  j["range_tiles"] = restaurant.economy().range_tiles();

  // Demographic arrays (6 values each)
  j["furniture_appeal"] = restaurant.economy().furniture_appeal().data();
  j["menu_factor"] = restaurant.economy().menu_factor().data();
  j["competitive_strength"] =
      restaurant.economy().competitive_strength().data();
  j["customer_counts"] = restaurant.economy().customer_counts().data();

  // Competition data
  j["demand_ratio"] = restaurant.economy().demand_ratio();
  j["competitor_count"] = restaurant.economy().competitor_count();
  j["strongest_competitor_x"] = restaurant.economy().strongest_competitor_x();
  j["strongest_competitor_y"] = restaurant.economy().strongest_competitor_y();
  j["competition_score"] = restaurant.economy().competition_score();

  // Jukebox/TV settings (player choices)
  j["jukebox_music"] = restaurant.economy().jukebox_music();
  j["tv_channel"] = restaurant.economy().tv_channel();

  // Save inventory
  j["inventory"] = restaurant_inventory_to_json(restaurant.inventory());

  // Save pizza quality (derived, but needed immediately after load)
  j["pizza_quality_rating"] = restaurant.pizza_quality_rating();
  const auto& pqf = restaurant.pizza_quality_factors();
  j["pizza_quality_factors"] = {pqf.ingredients, pqf.chef, pqf.oven,
                                pqf.contamination};

  // Save sabotage contamination and reputation damage slots
  j["sabotage_contamination"] = restaurant.sabotage_contamination();
  j["event_reputation_penalties"] = restaurant.event_reputation_penalties();
  j["event_reputation_recovery"] = restaurant.event_reputation_recovery();

  return j;
}

// Property pointer must be set afterwards by the caller
std::unique_ptr<Restaurant> json_to_restaurant(const json& j,
                                               Property* property) {
  core::RestaurantId rid{j["id"].get<int>()};
  std::string name = j["name"].get<std::string>();
  Size kitchen_size{j["kitchen_size"]["w"].get<int>(),
                    j["kitchen_size"]["h"].get<int>()};

  auto restaurant =
      std::make_unique<Restaurant>(property, kitchen_size, rid, name);

  // Restore open state
  if (j.contains("wants_open")) {
    restaurant->wants_open(j["wants_open"].get<bool>());
    int forced = j.value("forced_days_remaining", 0);
    if (forced > 0) {
      restaurant->force_close(forced);
    }
  }
  // Legacy save compatibility: convert old enum-based format
  else if (j.contains("open_status")) {
    int status = j["open_status"].get<int>();
    // 0=Open, 1=ClosedOutOfHours, 2=ClosedManually, 3=ClosedForced
    restaurant->wants_open(status == 0 || status == 1);
    if (status == 3) {
      restaurant->force_close(j.value("forced_days_closed", 1));
    }
  }

  // Restore menu
  if (j.contains("menu")) {
    auto menu = json_to_menu(j["menu"]);
    restaurant->menu() = std::move(menu);
  }

  // Restore cooks
  if (j.contains("cooks")) {
    auto& cooks = restaurant->cooks();
    for (size_t i = 0; i < j["cooks"].size() && i < cooks.size(); ++i) {
      if (!j["cooks"][i].is_null()) {
        cooks[i] = json_to_staff(j["cooks"][i]);
      }
    }
  }

  // Restore waiters
  if (j.contains("waiters")) {
    auto& waiters = restaurant->waiters();
    for (size_t i = 0; i < j["waiters"].size() && i < waiters.size(); ++i) {
      if (!j["waiters"][i].is_null()) {
        waiters[i] = json_to_staff(j["waiters"][i]);
      }
    }
  }

  // Restore manager
  if (j.contains("manager") && !j["manager"].is_null()) {
    restaurant->manager() = json_to_staff(j["manager"]);
  }

  // Restore layout
  if (j.contains("layout") && !j["layout"].is_null()) {
    auto layout = json_to_restaurant_layout(j["layout"]);
    restaurant->layout(std::move(layout));
  }

  // Restore stats
  if (j.contains("stats")) {
    restaurant->stats() = json_to_restaurant_stats(j["stats"]);
  }

  // Restore operating hours
  if (j.contains("opening_time")) {
    core::time::TimeOfDay opening{j["opening_time"]["hours"].get<int>(),
                                  j["opening_time"]["minutes"].get<int>()};
    restaurant->set_opening_time(opening);
  }
  if (j.contains("closing_time")) {
    core::time::TimeOfDay closing{j["closing_time"]["hours"].get<int>(),
                                  j["closing_time"]["minutes"].get<int>()};
    restaurant->set_closing_time(closing);
  }

  // Restore full economy state
  if (j.contains("total_popularity")) {
    restaurant->economy().total_popularity(j["total_popularity"].get<int>());
  }

  // Support both old "quality_multiplier" and new "quality_rating" field names
  if (j.contains("quality_rating")) {
    restaurant->economy().quality_rating(j["quality_rating"].get<int>());
  }
  else if (j.contains("quality_multiplier")) {
    restaurant->economy().quality_rating(j["quality_multiplier"].get<int>());
  }

  if (j.contains("rejection_rate")) {
    restaurant->economy().rejection_rate(j["rejection_rate"].get<int>());
  }

  if (j.contains("range_raw")) {
    restaurant->economy().range_raw(j["range_raw"].get<int>());
  }
  if (j.contains("range_tiles")) {
    restaurant->economy().range_tiles(j["range_tiles"].get<int>());
  }

  // Restore demographic arrays
  if (j.contains("furniture_appeal")) {
    auto& fa = restaurant->economy().furniture_appeal();
    for (size_t i = 0; i < 6 && i < j["furniture_appeal"].size(); ++i) {
      fa.data()[i] = j["furniture_appeal"][i].get<uint8_t>();
    }
  }
  if (j.contains("menu_factor")) {
    auto& mf = restaurant->economy().menu_factor();
    for (size_t i = 0; i < 6 && i < j["menu_factor"].size(); ++i) {
      mf.data()[i] = j["menu_factor"][i].get<uint8_t>();
    }
  }
  if (j.contains("competitive_strength")) {
    auto& cs = restaurant->economy().competitive_strength();
    for (size_t i = 0; i < 6 && i < j["competitive_strength"].size(); ++i) {
      cs.data()[i] = j["competitive_strength"][i].get<uint8_t>();
    }
  }
  if (j.contains("customer_counts")) {
    auto& cc = restaurant->economy().customer_counts();
    for (size_t i = 0; i < 6 && i < j["customer_counts"].size(); ++i) {
      cc.data()[i] = j["customer_counts"][i].get<int>();
    }
  }

  // Restore competition data
  if (j.contains("demand_ratio")) {
    restaurant->economy().demand_ratio(j["demand_ratio"].get<int>());
  }
  if (j.contains("competitor_count")) {
    restaurant->economy().competitor_count(j["competitor_count"].get<int>());
  }
  if (j.contains("strongest_competitor_x")) {
    restaurant->economy().strongest_competitor_x(
        j["strongest_competitor_x"].get<int>());
  }
  if (j.contains("strongest_competitor_y")) {
    restaurant->economy().strongest_competitor_y(
        j["strongest_competitor_y"].get<int>());
  }
  if (j.contains("competition_score")) {
    restaurant->economy().competition_score(
        j["competition_score"].get<int16_t>());
  }

  // Restore jukebox/TV settings
  if (j.contains("jukebox_music")) {
    restaurant->economy().jukebox_music(j["jukebox_music"].get<int>());
  }
  if (j.contains("tv_channel")) {
    restaurant->economy().tv_channel(j["tv_channel"].get<int>());
  }

  // Restore inventory
  if (j.contains("inventory")) {
    restaurant->inventory() = json_to_restaurant_inventory(j["inventory"]);
  }

  // Restore pizza quality
  if (j.contains("pizza_quality_rating")) {
    restaurant->set_pizza_quality_rating(
        j["pizza_quality_rating"].get<uint8_t>());
  }
  if (j.contains("pizza_quality_factors")) {
    const auto& pqf = j["pizza_quality_factors"];
    if (pqf.size() == 4) {
      PizzaQualityFactors factors;
      factors.ingredients = pqf[0].get<int8_t>();
      factors.chef = pqf[1].get<int8_t>();
      factors.oven = pqf[2].get<int8_t>();
      factors.contamination = pqf[3].get<int8_t>();
      restaurant->set_pizza_quality_factors(factors);
    }
  }

  // Restore sabotage contamination and reputation damage slots
  if (j.contains("sabotage_contamination")) {
    restaurant->set_sabotage_contamination(
        j["sabotage_contamination"].get<int8_t>());
  }
  if (j.contains("event_reputation_penalties")) {
    auto& penalties = restaurant->event_reputation_penalties();
    for (size_t i = 0; i < 4 && i < j["event_reputation_penalties"].size();
         ++i) {
      penalties[i] = j["event_reputation_penalties"][i].get<int8_t>();
    }
  }
  if (j.contains("event_reputation_recovery")) {
    auto& recovery = restaurant->event_reputation_recovery();
    for (size_t i = 0; i < 4 && i < j["event_reputation_recovery"].size();
         ++i) {
      recovery[i] = j["event_reputation_recovery"][i].get<int8_t>();
    }
  }

  return restaurant;
}

json pizza_sales_data_to_json(const core::restaurant::PizzaSalesData& data) {
  json j;
  j["pizza_id"] = data.pizza_id.value;
  j["count_sold"] = data.count_sold;
  j["revenue"] = data.revenue.cents;
  j["ingredient_cost"] = data.ingredient_cost.cents;
  j["profit"] = data.profit.cents;
  return j;
}

core::restaurant::PizzaSalesData json_to_pizza_sales_data(const json& j) {
  core::restaurant::PizzaSalesData data;
  data.pizza_id = core::RecipeId{j["pizza_id"].get<int>()};
  data.count_sold = j["count_sold"].get<int>();
  data.revenue = Money{j["revenue"].get<int64_t>()};
  data.ingredient_cost = Money{j["ingredient_cost"].get<int64_t>()};
  data.profit = Money{j["profit"].get<int64_t>()};
  return data;
}

json daily_stats_to_json(const core::restaurant::DailyStats& stats) {
  json j;

  // Order tracking
  j["orders_generated"] = stats.orders_generated;
  j["orders_fulfilled"] = stats.orders_fulfilled;
  j["orders_lost"] = stats.orders_lost;
  j["lost_due_to_capacity"] = stats.lost_due_to_capacity;
  j["lost_due_to_ingredients"] = stats.lost_due_to_ingredients;

  // Financial
  j["revenue"] = stats.revenue.cents;
  j["ingredient_costs"] = stats.ingredient_costs.cents;
  j["gross_profit"] = stats.gross_profit.cents;

  // Ingredient costs by category (consumption tracking)
  json costs_by_category = json::array();
  for (const auto& cost : stats.ingredient_costs_by_category) {
    costs_by_category.push_back(cost.cents);
  }
  j["ingredient_costs_by_category"] = costs_by_category;

  // Ingredient purchases by category (cash basis tracking)
  json purchases_by_category = json::array();
  for (const auto& cost : stats.ingredient_purchases_by_category) {
    purchases_by_category.push_back(cost.cents);
  }
  j["ingredient_purchases_by_category"] = purchases_by_category;

  // Total ingredient purchases
  j["ingredient_purchases"] = stats.ingredient_purchases.cents;

  // Furniture purchases
  j["furniture_purchases"] = stats.furniture_purchases.cents;

  j["fixed_costs"] = stats.fixed_costs.cents;
  j["net_profit"] = stats.net_profit.cents;

  // Operational
  j["avg_quality"] = stats.avg_quality;
  j["capacity_utilization"] = stats.capacity_utilization;

  // Per-pizza sales
  json pizza_sales_obj = json::object();
  for (const auto& [pizza_id, sales_data] : stats.pizza_sales) {
    pizza_sales_obj[std::to_string(pizza_id.value)] =
        pizza_sales_data_to_json(sales_data);
  }
  j["pizza_sales"] = pizza_sales_obj;

  // Demand value tracking
  j["total_order_value"] = stats.total_order_value.cents;
  j["order_value_count"] = stats.order_value_count;

  return j;
}

core::restaurant::DailyStats json_to_daily_stats(const json& j) {
  core::restaurant::DailyStats stats;

  // Order tracking
  stats.orders_generated = j.value("orders_generated", 0);
  stats.orders_fulfilled = j.value("orders_fulfilled", 0);
  stats.orders_lost = j.value("orders_lost", 0);
  stats.lost_due_to_capacity = j.value("lost_due_to_capacity", 0);
  stats.lost_due_to_ingredients = j.value("lost_due_to_ingredients", 0);

  // Financial
  stats.revenue = Money{j.value("revenue", 0LL)};
  stats.ingredient_costs = Money{j.value("ingredient_costs", 0LL)};
  stats.gross_profit = Money{j.value("gross_profit", 0LL)};

  // Ingredient costs by category (consumption tracking)
  if (j.contains("ingredient_costs_by_category")) {
    for (size_t i = 0; i < 4 && i < j["ingredient_costs_by_category"].size();
         ++i) {
      stats.ingredient_costs_by_category[i] =
          Money{j["ingredient_costs_by_category"][i].get<int64_t>()};
    }
  }

  // Ingredient purchases by category (cash basis tracking)
  if (j.contains("ingredient_purchases_by_category")) {
    for (size_t i = 0;
         i < 4 && i < j["ingredient_purchases_by_category"].size(); ++i) {
      stats.ingredient_purchases_by_category[i] =
          Money{j["ingredient_purchases_by_category"][i].get<int64_t>()};
    }
  }

  // Total ingredient purchases
  stats.ingredient_purchases = Money{j.value("ingredient_purchases", 0LL)};

  // Furniture purchases
  stats.furniture_purchases = Money{j.value("furniture_purchases", 0LL)};

  stats.fixed_costs = Money{j.value("fixed_costs", 0LL)};
  stats.net_profit = Money{j.value("net_profit", 0LL)};

  // Operational
  stats.avg_quality = j.value("avg_quality", 0);
  stats.capacity_utilization = j.value("capacity_utilization", 0);

  // Per-pizza sales
  if (j.contains("pizza_sales")) {
    for (auto it = j["pizza_sales"].begin(); it != j["pizza_sales"].end();
         ++it) {
      core::RecipeId pizza_id{std::stoi(it.key())};
      stats.pizza_sales[pizza_id] = json_to_pizza_sales_data(it.value());
    }
  }

  // Demand value tracking
  stats.total_order_value = Money{j.value("total_order_value", 0LL)};
  stats.order_value_count = j.value("order_value_count", 0);

  return stats;
}

json restaurant_stats_to_json(const core::restaurant::RestaurantStats& stats) {
  json j;

  // Current days
  j["today"] = daily_stats_to_json(stats.today);
  j["yesterday"] = daily_stats_to_json(stats.yesterday);
  j["day_before_yesterday"] = daily_stats_to_json(stats.day_before_yesterday);

  // All-time totals
  j["total_pizzas_sold"] = stats.total_pizzas_sold;
  j["total_revenue"] = stats.total_revenue.cents;
  j["total_profit"] = stats.total_profit.cents;

  // Complete history
  json history_array = json::array();
  for (const auto& day_stats : stats.history) {
    history_array.push_back(daily_stats_to_json(day_stats));
  }
  j["history"] = history_array;

  return j;
}

core::restaurant::RestaurantStats json_to_restaurant_stats(const json& j) {
  core::restaurant::RestaurantStats stats;

  // Current days
  if (j.contains("today")) {
    stats.today = json_to_daily_stats(j["today"]);
  }
  if (j.contains("yesterday")) {
    stats.yesterday = json_to_daily_stats(j["yesterday"]);
  }
  if (j.contains("day_before_yesterday")) {
    stats.day_before_yesterday = json_to_daily_stats(j["day_before_yesterday"]);
  }

  // All-time totals
  stats.total_pizzas_sold = j.value("total_pizzas_sold", 0);
  stats.total_revenue = Money{j.value("total_revenue", 0LL)};
  stats.total_profit = Money{j.value("total_profit", 0LL)};

  // Complete history
  if (j.contains("history")) {
    for (const auto& day_json : j["history"]) {
      stats.history.push_back(json_to_daily_stats(day_json));
    }
  }

  return stats;
}

// IngredientStock serialization
json ingredient_stock_to_json(const core::restaurant::IngredientStock& stock) {
  json j;
  j["quantity"] = stock.quantity;
  j["weighted_avg_quality"] = stock.weighted_avg_quality;
  j["weighted_avg_cost"] = stock.weighted_avg_cost.cents;
  return j;
}

core::restaurant::IngredientStock json_to_ingredient_stock(const json& j) {
  core::restaurant::IngredientStock stock;
  stock.quantity = j["quantity"].get<int>();
  stock.weighted_avg_quality = j["weighted_avg_quality"].get<int>();
  stock.weighted_avg_cost = Money{j["weighted_avg_cost"].get<int64_t>()};
  return stock;
}

// RestaurantInventory serialization
json restaurant_inventory_to_json(
    const core::restaurant::RestaurantInventory& inventory) {
  json j = json::array();
  for (const auto& stock : inventory.all_stocks()) {
    j.push_back(ingredient_stock_to_json(stock));
  }
  return j;
}

core::restaurant::RestaurantInventory
json_to_restaurant_inventory(const json& j) {
  core::restaurant::RestaurantInventory inventory;
  auto& stocks = inventory.all_stocks();

  for (size_t i = 0; i < j.size() && i < stocks.size(); ++i) {
    stocks[i] = json_to_ingredient_stock(j[i]);
  }

  return inventory;
}

} // namespace save
