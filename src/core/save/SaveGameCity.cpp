/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/save/SaveGameCity.h"

#include "core/city/City.h"
#include "core/city/Property.h"
#include "core/food/Ingredient.h"
#include "core/food/IngredientCategory.h"
#include "core/newspaper/NewspaperEventSystem.h"
#include "core/newspaper/NewspaperState.h"
#include "core/supply/FoodSupplier.h"
#include "engine/Logger.h"

namespace save {

json property_to_json(const Property& property) {
  json j;
  j["index"] = property.index();
  j["type"] = static_cast<int>(property.type());
  j["size"] = {{"w", property.size().w}, {"h", property.size().h}};
  j["realtor_id"] = property.realtor_id();
  j["status"] = static_cast<int>(property.status());

  // Save owner/tenant if any
  if (property.owner_or_tenant().has_value()) {
    j["owner_or_tenant"] = property.owner_or_tenant()->value;
  }
  else {
    j["owner_or_tenant"] = nullptr;
  }

  // Save restaurant link if any
  if (property.restaurant_id().has_value()) {
    j["restaurant_id"] = property.restaurant_id()->value;
  }
  else {
    j["restaurant_id"] = nullptr;
  }

  // Save potential customers per demographic (needed for demand calculation)
  j["potential_customers"] = property.potential_customers().data();

  return j;
}

void json_to_property(const json& j, Property& property) {
  // Restore size
  if (j.contains("size")) {
    property.size(Size{j["size"]["w"].get<int>(), j["size"]["h"].get<int>()});
  }

  // Restore realtor_id (important to preserve randomly assigned realtors)
  if (j.contains("realtor_id")) {
    property.realtor_id(j["realtor_id"].get<int>());
  }

  // Restore status
  if (j.contains("status")) {
    property.status(static_cast<PremiseStatus>(j["status"].get<int>()));
  }

  // Restore potential customers per demographic
  if (j.contains("potential_customers")) {
    auto& pc = property.potential_customers();
    for (size_t i = 0; i < 6 && i < j["potential_customers"].size(); ++i) {
      pc.data()[i] = j["potential_customers"][i].get<int>();
    }
  }

  // Restore owner/tenant (must explicitly clear if null)
  if (j.contains("owner_or_tenant")) {
    if (!j["owner_or_tenant"].is_null()) {
      property.owner_or_tenant(core::PlayerId{j["owner_or_tenant"].get<int>()});
    }
    else {
      // Clear owner/tenant if the save has null
      property.clear_owner();
    }
  }

  // Restore restaurant link (must explicitly clear if null)
  if (j.contains("restaurant_id")) {
    if (!j["restaurant_id"].is_null()) {
      property.restaurant_id(core::RestaurantId{j["restaurant_id"].get<int>()});
    }
    else {
      // Clear restaurant link if the save has null
      property.clear_restaurant();
    }
  }
}

json supplier_to_json(const FoodSupplier& supplier) {
  json j;
  j["id"] = supplier.id();

  // Save stock levels for all ingredients this supplier handles
  json stock_array = json::array();
  for (auto ing_type :
       core::food::CategoryRegistry::range(supplier.category())) {
    stock_array.push_back(supplier.stock(ing_type));
  }
  j["stock"] = stock_array;

  // Save per-player relationships
  json relationships = json::array();
  for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    core::PlayerId pid{i};
    const auto& rel = supplier.relationship(pid);
    json rel_j;
    rel_j["total_spent"] = rel.total_spent.cents;
    rel_j["contract_count"] = rel.contract_count;
    rel_j["relationship_score"] = rel.relationship_score;
    rel_j["frustration"] = rel.frustration;
    rel_j["last_delivery_days"] = rel.last_delivery_days;
    rel_j["last_payment_freq"] = rel.last_payment_freq;
    rel_j["credit_history"] = rel.credit_history;
    relationships.push_back(rel_j);
  }
  j["relationships"] = relationships;

  return j;
}

void json_to_supplier(const json& j, FoodSupplier& supplier) {
  // Restore stock levels
  if (j.contains("stock")) {
    int idx = 0;
    for (auto ing_type :
         core::food::CategoryRegistry::range(supplier.category())) {
      if (idx < static_cast<int>(j["stock"].size())) {
        supplier.set_stock(ing_type, j["stock"][idx].get<int>());
        idx++;
      }
    }
  }

  // Restore per-player relationships
  if (j.contains("relationships")) {
    int idx = 0;
    for (const auto& rel_json : j["relationships"]) {
      if (idx >= NUMBER_OF_PLAYERS) {
        break;
      }
      core::PlayerId pid{idx};
      auto& rel = supplier.relationship(pid);
      if (rel_json.contains("total_spent")) {
        rel.total_spent = Money{rel_json["total_spent"].get<int64_t>()};
      }
      if (rel_json.contains("contract_count")) {
        rel.contract_count = rel_json["contract_count"].get<int>();
      }
      if (rel_json.contains("relationship_score")) {
        rel.relationship_score = rel_json["relationship_score"].get<uint8_t>();
      }
      if (rel_json.contains("frustration")) {
        rel.frustration = rel_json["frustration"].get<uint8_t>();
      }
      if (rel_json.contains("last_delivery_days")) {
        rel.last_delivery_days = rel_json["last_delivery_days"].get<uint8_t>();
      }
      if (rel_json.contains("last_payment_freq")) {
        rel.last_payment_freq = rel_json["last_payment_freq"].get<uint16_t>();
      }
      if (rel_json.contains("credit_history")) {
        rel.credit_history = rel_json["credit_history"].get<uint8_t>();
      }
      idx++;
    }
  }
}

namespace {

json pizza_chart_entry_to_json(const PizzaChartEntry& entry) {
  return {{"previous_position", entry.previous_position},
          {"player_id", entry.player_id.value},
          {"pizza_name", entry.pizza_name},
          {"player_name", entry.player_name},
          {"score", entry.score}};
}

PizzaChartEntry json_to_pizza_chart_entry(const json& j) {
  PizzaChartEntry e;
  e.previous_position = j.value("previous_position", uint8_t{0});
  e.player_id = core::PlayerId{j.value("player_id", 0)};
  e.pizza_name = j.value("pizza_name", std::string{});
  e.player_name = j.value("player_name", std::string{});
  e.score = j.value("score", 0);
  return e;
}

} // namespace

json pizza_charts_to_json(const PizzaCharts& charts) {
  json j;
  json entries_array = json::array();
  for (const auto& entry : charts.entries()) {
    entries_array.push_back(pizza_chart_entry_to_json(entry));
  }
  j["entries"] = entries_array;

  json previous_array = json::array();
  for (const auto& entry : charts.previous()) {
    previous_array.push_back(pizza_chart_entry_to_json(entry));
  }
  j["previous"] = previous_array;
  return j;
}

void json_to_pizza_charts(const json& j, PizzaCharts& charts) {
  if (j.contains("entries")) {
    std::vector<PizzaChartEntry> entries;
    for (const auto& e : j["entries"]) {
      entries.push_back(json_to_pizza_chart_entry(e));
    }
    charts.set_entries(std::move(entries));
  }
  if (j.contains("previous")) {
    std::vector<PizzaChartEntry> previous;
    for (const auto& e : j["previous"]) {
      previous.push_back(json_to_pizza_chart_entry(e));
    }
    charts.set_previous(std::move(previous));
  }
}

json control_map_to_json(const ControlMap& cm) {
  json j;
  json owners = json::array();
  for (int i = 0; i < CONTROL_MAP_CELLS; ++i) {
    auto owner = cm.cell_owner(i);
    owners.push_back(owner.has_value() ? owner->value : -1);
  }
  j["cell_owners"] = owners;
  j["player_percentages"] = cm.player_percentages();
  return j;
}

void json_to_control_map(const json& j, ControlMap& cm) {
  if (j.contains("cell_owners")) {
    std::array<std::optional<core::PlayerId>, CONTROL_MAP_CELLS> owners{};
    int i = 0;
    for (const auto& val : j["cell_owners"]) {
      int v = val.get<int>();
      if (v >= 0) {
        owners[i] = core::PlayerId{v};
      }
      if (++i >= CONTROL_MAP_CELLS) {
        break;
      }
    }
    cm.set_cell_owners(owners);
  }
  if (j.contains("player_percentages")) {
    std::array<float, NUMBER_OF_PLAYERS> pcts{};
    const auto& arr = j["player_percentages"];
    for (int i = 0; i < NUMBER_OF_PLAYERS && i < static_cast<int>(arr.size());
         ++i) {
      pcts[i] = arr[i].get<float>();
    }
    cm.set_player_percentages(pcts);
  }
}

json economy_trend_to_json(const EconomyTrend& et) {
  json j;
  j["current"] = et.current();
  j["target"] = et.target();
  j["days_remaining"] = et.days_remaining();
  j["history_count"] = et.history_count();
  j["history"] = et.history();
  return j;
}

void json_to_economy_trend(const json& j, EconomyTrend& et) {
  if (j.contains("current")) {
    et.set_current(j["current"].get<int>());
  }
  if (j.contains("target")) {
    et.set_target(j["target"].get<int>());
  }
  if (j.contains("days_remaining")) {
    et.set_days_remaining(j["days_remaining"].get<int>());
  }
  if (j.contains("history_count")) {
    et.set_history_count(j["history_count"].get<int>());
  }
  if (j.contains("history")) {
    std::array<int, 60> history{};
    auto arr = j["history"];
    for (int i = 0; i < 60 && i < static_cast<int>(arr.size()); ++i) {
      history[i] = arr[i].get<int>();
    }
    et.set_history(history);
  }
}

json city_to_json(const City& city) {
  json j;
  j["id"] = city.id().value;

  // Save all properties
  json properties_array = json::array();
  for (const auto* property : city.properties()) {
    properties_array.push_back(property_to_json(*property));
  }
  j["properties"] = properties_array;

  // Save blacklist
  json blacklist = json::object();
  for (const auto& [char_id, days] : city.blacklist()) {
    blacklist[std::to_string(char_id)] = days;
  }
  j["blacklist"] = blacklist;

  // Save all suppliers
  json suppliers_array = json::array();
  for (const auto& supplier : city.suppliers()) {
    suppliers_array.push_back(supplier_to_json(*supplier));
  }
  j["suppliers"] = suppliers_array;

  // Save ingredient trends (active slots only)
  const auto& trend_sys = city.ingredient_trend_system();
  const auto& durations = trend_sys.durations();
  json trends_array = json::array();
  for (int i = 0; i < max_ingredient_count; ++i) {
    if (durations[i] == TrendDuration::None) {
      continue;
    }
    auto type = static_cast<core::food::IngredientType>(i);
    json slot_j;
    slot_j["slot"] = i;
    slot_j["current"] = trend_sys.get_trend(type);
    slot_j["target"] = trend_sys.targets()[i];
    slot_j["duration"] = static_cast<int>(durations[i]);
    slot_j["approaching"] = trend_sys.approaching_flags()[i];
    trends_array.push_back(slot_j);
  }
  j["ingredient_trends"] = trends_array;

  // Save pizza charts (current + previous entries for rank-change arrows)
  j["pizza_charts"] = pizza_charts_to_json(city.pizza_charts());
  j["control_map"] = control_map_to_json(city.control_map());
  j["economy_trend"] = economy_trend_to_json(city.economy_trend());

  // Save known arms dealers per player
  json known_arms_dealers_array = json::array();
  for (int player_idx = 0; player_idx < NUMBER_OF_PLAYERS; ++player_idx) {
    core::PlayerId player_id{player_idx};
    json player_data;
    player_data["known_count"] = city.known_arms_dealer_count(player_id);

    json actor_ids_array = json::array();
    for (ActorId actor_id : city.known_arms_dealers(player_id)) {
      actor_ids_array.push_back(actor_id);
    }
    player_data["known_actor_ids"] = actor_ids_array;

    known_arms_dealers_array.push_back(player_data);
  }
  j["known_arms_dealers"] = known_arms_dealers_array;

  // Save newspaper state
  const auto& ns = city.newspaper_state();
  json newspaper_j;
  newspaper_j["article_id"] = ns.article_id;
  newspaper_j["cooldown"] = ns.cooldown;
  newspaper_j["polarity"] = ns.polarity;
  newspaper_j["category"] = ns.category;
  if (ns.target_ingredient) {
    newspaper_j["target_ingredient"] = static_cast<int>(*ns.target_ingredient);
  }
  else {
    newspaper_j["target_ingredient"] = nullptr;
  }
  j["newspaper_state"] = newspaper_j;

  // Save scheduled newspaper events
  const auto& event_sys = city.newspaper_event_system();
  const auto& sched_types = event_sys.scheduled_types();
  const auto& sched_countdowns = event_sys.scheduled_countdowns();
  json scheduled_array = json::array();
  for (int i = 0; i < 50; ++i) {
    if (static_cast<int>(sched_types[i]) != 0 || sched_countdowns[i] != 0) {
      scheduled_array.push_back({{"slot", i},
                                 {"event", static_cast<int>(sched_types[i])},
                                 {"countdown", sched_countdowns[i]}});
    }
  }
  j["newspaper_scheduled"] = scheduled_array;

  return j;
}

void json_to_city(const json& j, City& city) {
  // Restore all properties
  if (j.contains("properties")) {
    for (const auto& prop_json : j["properties"]) {
      int index = prop_json["index"].get<int>();
      Property* property = city.property_by_index(index);
      if (property) {
        json_to_property(prop_json, *property);
      }
      else {
        Logger::warn("Property index {} not found in city {}", index,
                     city.id().value);
      }
    }
  }

  // Restore blacklist
  if (j.contains("blacklist")) {
    for (const auto& [key, value] : j["blacklist"].items()) {
      uint8_t char_id = std::stoi(key);
      int days = value.get<int>();
      city.add_to_blacklist(char_id, days);
    }
  }

  // Restore suppliers
  if (j.contains("suppliers")) {
    for (const auto& supp_json : j["suppliers"]) {
      uint8_t supplier_id = supp_json["id"].get<uint8_t>();

      // Find the supplier by ID
      FoodSupplier* supplier = nullptr;
      for (const auto& s : city.suppliers()) {
        if (s->id() == supplier_id) {
          supplier = s.get();
          break;
        }
      }

      if (supplier) {
        json_to_supplier(supp_json, *supplier);
      }
      else {
        Logger::warn("Supplier ID {} not found in city {}", supplier_id,
                     city.id().value);
      }
    }
  }

  // Restore ingredient trends
  if (j.contains("ingredient_trends")) {
    auto& trend_sys = city.ingredient_trend_system();
    trend_sys.clear();
    for (const auto& slot_j : j["ingredient_trends"]) {
      int slot = slot_j["slot"].get<int>();
      auto type = static_cast<core::food::IngredientType>(slot);
      TrendEffect effect;
      effect.initial_value = slot_j["current"].get<int>();
      effect.control = slot_j["target"].get<int>();
      effect.duration =
          static_cast<TrendDuration>(slot_j["duration"].get<int>());
      trend_sys.apply_effect(type, effect);
      if (!slot_j["approaching"].get<bool>()) {
        trend_sys.reset_approaching(type);
      }
    }
  }

  // Restore pizza charts
  if (j.contains("pizza_charts")) {
    json_to_pizza_charts(j["pizza_charts"], city.pizza_charts());
  }

  // Restore control map
  if (j.contains("control_map")) {
    json_to_control_map(j["control_map"], city.control_map());
  }

  // Restore economy trend
  if (j.contains("economy_trend")) {
    json_to_economy_trend(j["economy_trend"], city.economy_trend());
  }

  // Restore known arms dealers per player
  if (j.contains("known_arms_dealers")) {
    int player_idx = 0;
    for (const auto& player_data : j["known_arms_dealers"]) {
      if (player_idx >= NUMBER_OF_PLAYERS) {
        break;
      }

      core::PlayerId player_id{player_idx};

      // Clear existing data first (removes factory-initialized data)
      city.clear_known_arms_dealers(player_id);

      // Restore saved known arms dealers
      if (player_data.contains("known_actor_ids")) {
        for (const auto& actor_id_json : player_data["known_actor_ids"]) {
          ActorId actor_id = actor_id_json.get<int>();
          city.add_known_arms_dealer(player_id, actor_id);
        }
      }

      player_idx++;
    }
  }

  // Restore newspaper state
  if (j.contains("newspaper_state")) {
    const auto& ns_j = j["newspaper_state"];
    auto& ns = city.newspaper_state();
    ns.article_id = ns_j.value("article_id", uint8_t{0});
    ns.cooldown = ns_j.value("cooldown", 0);
    ns.polarity = ns_j.value("polarity", int8_t{0});
    ns.category = ns_j.value("category", uint8_t{0});
    if (ns_j.contains("target_ingredient") &&
        !ns_j["target_ingredient"].is_null()) {
      ns.target_ingredient = static_cast<core::food::IngredientType>(
          ns_j["target_ingredient"].get<int>());
    }
    else {
      ns.target_ingredient = std::nullopt;
    }
  }

  // Restore scheduled newspaper events
  if (j.contains("newspaper_scheduled")) {
    using core::newspaper::NewspaperEvent;
    std::array<NewspaperEvent, 50> types{};
    std::array<int, 50> countdowns{};
    for (const auto& entry : j["newspaper_scheduled"]) {
      int slot = entry["slot"].get<int>();
      if (slot >= 0 && slot < 50) {
        types[slot] = static_cast<NewspaperEvent>(entry["event"].get<int>());
        countdowns[slot] = entry["countdown"].get<int>();
      }
    }
    city.newspaper_event_system().restore_scheduled(types, countdowns);
  }
}

} // namespace save
