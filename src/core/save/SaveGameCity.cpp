/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/save/SaveGameCity.h"

#include <algorithm>
#include <array>

#include "core/city/CentralDepot.h"
#include "core/city/City.h"
#include "core/city/JobMarket.h"
#include "core/city/MayorOffice.h"
#include "core/city/Property.h"
#include "core/food/IngredientCategory.h"
#include "core/marketing/AdvertisingCampaign.h"
#include "core/newspaper/NewspaperEventSystem.h"
#include "core/newspaper/NewspaperState.h"
#include "core/save/SaveGameDate.h"
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
    rel_j["delivery_issues_flag"] = rel.delivery_issues_flag;
    rel_j["outstanding_debt"] = rel.outstanding_debt.cents;
    rel_j["last_contract_sum"] = rel.last_contract_sum.cents;
    rel_j["missed_payment_penalty"] = rel.missed_payment_penalty;
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
      if (rel_json.contains("delivery_issues_flag")) {
        rel.delivery_issues_flag =
            rel_json["delivery_issues_flag"].get<uint8_t>();
      }
      if (rel_json.contains("outstanding_debt")) {
        rel.outstanding_debt =
            Money{rel_json["outstanding_debt"].get<int64_t>()};
      }
      if (rel_json.contains("last_contract_sum")) {
        rel.last_contract_sum =
            Money{rel_json["last_contract_sum"].get<int64_t>()};
      }
      if (rel_json.contains("missed_payment_penalty")) {
        rel.missed_payment_penalty =
            rel_json["missed_payment_penalty"].get<uint8_t>();
      }
      idx++;
    }
  }
}

namespace {

json pizza_chart_entry_to_json(const PizzaChartEntry& entry) {
  return {{"previous_position", entry.previous_position},
          {"player_id", entry.player_id.value},
          {"pizza_ref", entry.pizza_ref.value},
          {"score", entry.score}};
}

PizzaChartEntry json_to_pizza_chart_entry(const json& j) {
  PizzaChartEntry e;
  e.previous_position = j.value("previous_position", uint8_t{0});
  e.player_id = core::PlayerId{j.value("player_id", 0)};
  e.pizza_ref = PizzaChartRef{j.value("pizza_ref", PizzaChartRef::unresolved)};
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
    const auto& arr = j["history"];
    for (int i = 0; i < 60 && i < static_cast<int>(arr.size()); ++i) {
      history[i] = arr[i].get<int>();
    }
    et.set_history(history);
  }
}

json mayor_office_to_json(const MayorOffice& mo) {
  json j;

  json favor = json::array();
  json investment = json::array();
  json trade = json::array();
  json fee_paid = json::array();
  for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    favor.push_back(mo.favor_score(i));
    investment.push_back(mo.investment_activity(i));
    trade.push_back(mo.trade_activity(i));
    fee_paid.push_back(mo.fee_paid(i));
  }
  j["favor_score"] = favor;
  j["investment_activity"] = investment;
  j["trade_activity"] = trade;
  j["fee_paid"] = fee_paid;

  json competition_wins = json::array();
  for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    competition_wins.push_back(mo.pizza_competition_wins(i));
  }
  j["pizza_competition_wins"] = competition_wins;

  j["public_bank_account"] = mo.public_bank_account();
  j["private_bank_account"] = mo.private_bank_account();
  if (mo.last_rotation_date()) {
    j["last_rotation_date"] = date_to_json(*mo.last_rotation_date());
  }

  j["appointment_day"] = mo.appointment_day();
  j["current_quarterly_award"] = static_cast<int>(mo.current_quarterly_award());
  j["award_announcement_day"] = mo.award_announcement_day();
  j["award_ceremony_day"] = mo.award_ceremony_day();

  return j;
}

void json_to_mayor_office(const json& j, MayorOffice& mo) {
  if (j.contains("favor_score")) {
    for (int i = 0;
         i < NUMBER_OF_PLAYERS && i < static_cast<int>(j["favor_score"].size());
         ++i) {
      mo.favor(i, j["favor_score"][i].get<int>());
    }
  }
  if (j.contains("investment_activity")) {
    for (int i = 0; i < NUMBER_OF_PLAYERS &&
                    i < static_cast<int>(j["investment_activity"].size());
         ++i) {
      mo.investment_activity(i, j["investment_activity"][i].get<int32_t>());
    }
  }
  if (j.contains("trade_activity")) {
    for (int i = 0; i < NUMBER_OF_PLAYERS &&
                    i < static_cast<int>(j["trade_activity"].size());
         ++i) {
      mo.trade_activity(i, j["trade_activity"][i].get<int32_t>());
    }
  }
  if (j.contains("fee_paid")) {
    mo.clear_fee_paid();
    for (int i = 0;
         i < NUMBER_OF_PLAYERS && i < static_cast<int>(j["fee_paid"].size());
         ++i) {
      if (j["fee_paid"][i].get<bool>()) {
        mo.fee_paid(i);
      }
    }
  }
  if (j.contains("pizza_competition_wins")) {
    for (int i = 0; i < NUMBER_OF_PLAYERS &&
                    i < static_cast<int>(j["pizza_competition_wins"].size());
         ++i) {
      mo.pizza_competition_wins(i, j["pizza_competition_wins"][i].get<int>());
    }
  }
  if (j.contains("public_bank_account")) {
    mo.public_bank_account(j["public_bank_account"].get<int>());
  }
  if (j.contains("private_bank_account")) {
    mo.private_bank_account(j["private_bank_account"].get<int>());
  }
  if (j.contains("last_rotation_date")) {
    mo.last_rotation_date(json_to_date(j["last_rotation_date"]));
  }
  if (j.contains("appointment_day")) {
    mo.appointment_day(j["appointment_day"].get<int>());
  }
  if (j.contains("current_quarterly_award")) {
    mo.current_quarterly_award(
        static_cast<MayorAwardType>(j["current_quarterly_award"].get<int>()));
  }
  if (j.contains("award_announcement_day")) {
    mo.award_announcement_day(j["award_announcement_day"].get<int>());
  }
  if (j.contains("award_ceremony_day")) {
    mo.award_ceremony_day(j["award_ceremony_day"].get<int>());
  }
}

static json depot_to_json(const core::city::CentralDepot& depot) {
  json j;
  j["capacity"] = depot.capacity();

  json stocks_array = json::array();
  const auto& all = depot.all_stocks();
  for (int i = 0; i < max_ingredient_count; ++i) {
    const auto& stock = all[i];
    if (stock.quantity > 0) {
      json s;
      s["i"] = i;
      s["qty"] = stock.quantity;
      s["quality"] = stock.weighted_avg_quality;
      s["cost"] = stock.weighted_avg_cost.cents;
      s["last_price"] = stock.last_price.cents;
      stocks_array.push_back(s);
    }
  }
  j["stocks"] = stocks_array;

  json prio_array = json::array();
  for (auto id : depot.delivery_priorities()) {
    prio_array.push_back(id.value);
  }
  j["delivery_priorities"] = prio_array;

  if (auto stolen = depot.stock_stolen()) {
    j["stock_stolen"] = static_cast<int>(*stolen);
  }
  return j;
}

static void json_to_depot(const json& j, core::city::CentralDepot& depot) {
  if (j.contains("capacity")) {
    int saved = j["capacity"].get<int>();
    int delta = saved - depot.capacity();
    if (delta > 0) {
      depot.add_capacity(delta);
    }
    else if (delta < 0) {
      depot.remove_capacity(-delta);
    }
  }
  if (j.contains("stocks")) {
    depot.all_stocks().fill({});
    for (const auto& s : j["stocks"]) {
      int i = s["i"].get<int>();
      if (i < 0 || i >= max_ingredient_count) {
        continue;
      }
      auto type = static_cast<core::food::IngredientType>(i);
      depot.add_ingredient(type, s.value("qty", 0), s.value("quality", 0),
                           Money{s.value("cost", int64_t{0})});
      depot.get_stock(type).last_price =
          Money{s.value("last_price", int64_t{0})};
    }
  }
  if (j.contains("delivery_priorities")) {
    std::vector<core::RestaurantId> prios;
    for (const auto& v : j["delivery_priorities"]) {
      prios.push_back(core::RestaurantId{v.get<int>()});
    }
    depot.delivery_priorities(std::move(prios));
  }
  if (j.contains("stock_stolen") && !j["stock_stolen"].is_null()) {
    depot.stock_stolen(
        static_cast<IngredientCategory>(j["stock_stolen"].get<int>()));
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

  // Save staff hiring blacklist
  json blacklist = json::object();
  for (const auto& [char_id, players] : city.staff_blacklist()) {
    json players_array = json::array();
    for (core::PlayerId p : players) {
      players_array.push_back(p.value);
    }
    blacklist[std::to_string(char_id)] = players_array;
  }
  j["blacklist"] = blacklist;

  // Save job market status array
  json market_array = json::array();
  for (const auto& s : city.job_market().slots()) {
    market_array.push_back(static_cast<uint8_t>(s));
  }
  j["job_market"] = market_array;

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

  json embargoes = json::array();
  for (auto cat : ALL_INGREDIENT_CATEGORIES) {
    embargoes.push_back(city.trade_embargo_counter(cat));
  }
  j["trade_embargoes"] = embargoes;

  // The rates are rolled per city when it is first built, so a reload would
  // otherwise offer the player different prices than the ones they saved on.
  json poster_rates = json::array();
  for (int i = 0; i < NUMBER_OF_POSTERS; ++i) {
    const auto& poster = city.poster_option(i);
    poster_rates.push_back(
        {{"effectiveness", poster.effectiveness}, {"cost", poster.cost.cents}});
  }
  j["poster_rates"] = poster_rates;

  json tv_ad_rates = json::array();
  for (int i = 0; i < NUMBER_OF_TV_ADS; ++i) {
    const auto& tv_ad = city.tv_ad_option(i);
    tv_ad_rates.push_back(
        {{"effectiveness", tv_ad.effectiveness}, {"cost", tv_ad.cost.cents}});
  }
  j["tv_ad_rates"] = tv_ad_rates;

  json vendor_spending = json::array();
  for (int player_idx = 0; player_idx < NUMBER_OF_PLAYERS; ++player_idx) {
    json per_player = json::object();
    for (const auto& [vendor_id, spent] :
         city.weekly_vendor_spending(core::PlayerId{player_idx})) {
      per_player[std::to_string(vendor_id)] = spent.cents;
    }
    vendor_spending.push_back(per_player);
  }
  j["weekly_vendor_spending"] = vendor_spending;

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

  // Save mayor office state
  j["mayor_office"] = mayor_office_to_json(city.mayor_office());

  // Save per-player central depots
  json depots_array = json::array();
  for (int player_idx = 0; player_idx < NUMBER_OF_PLAYERS; ++player_idx) {
    depots_array.push_back(
        depot_to_json(city.central_depot(core::PlayerId{player_idx})));
  }
  j["central_depots"] = depots_array;

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

  j["spoilage_rate"] = city.spoilage_rate();
  j["spoilage_duration_counter"] = city.spoilage_duration_counter();

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
      for (const auto& player : value) {
        city.blacklist_staff(char_id, core::PlayerId{player.get<int>()});
      }
    }
  }

  // Restore job market status array
  if (j.contains("job_market")) {
    const auto& arr = j["job_market"];
    std::vector<core::character::MarketStatus> slots;
    slots.reserve(arr.size());
    for (const auto& entry : arr) {
      slots.push_back(
          static_cast<core::character::MarketStatus>(entry.get<uint8_t>()));
    }
    city.job_market().load_slots(slots);
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

  city.spoilage_rate(j.value("spoilage_rate", 1));
  city.spoilage_duration_counter(j.value("spoilage_duration_counter", 0));

  // Restore mayor office state
  if (j.contains("mayor_office")) {
    json_to_mayor_office(j["mayor_office"], city.mayor_office());
  }

  // Restore per-player central depots
  if (j.contains("central_depots")) {
    int player_idx = 0;
    for (const auto& depot_j : j["central_depots"]) {
      if (player_idx >= NUMBER_OF_PLAYERS) {
        break;
      }
      json_to_depot(depot_j, city.central_depot(core::PlayerId{player_idx}));
      ++player_idx;
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

  if (j.contains("trade_embargoes")) {
    const auto& embargoes = j["trade_embargoes"];
    for (std::size_t c = 0;
         c < ALL_INGREDIENT_CATEGORIES.size() && c < embargoes.size(); ++c) {
      city.trade_embargo_counter(ALL_INGREDIENT_CATEGORIES[c],
                                 embargoes[c].get<int>());
    }
  }

  if (j.contains("poster_rates")) {
    const auto& saved = j["poster_rates"];
    std::array<AdvertisingRate, NUMBER_OF_POSTERS> posters{};
    for (std::size_t i = 0; i < posters.size() && i < saved.size(); ++i) {
      posters[i].effectiveness = saved[i]["effectiveness"].get<int>();
      posters[i].cost = Money{saved[i]["cost"].get<int64_t>()};
    }
    city.poster_rates(posters);
  }

  if (j.contains("tv_ad_rates")) {
    const auto& saved = j["tv_ad_rates"];
    std::array<AdvertisingRate, NUMBER_OF_TV_ADS> tv_ads{};
    for (std::size_t i = 0; i < tv_ads.size() && i < saved.size(); ++i) {
      tv_ads[i].effectiveness = saved[i]["effectiveness"].get<int>();
      tv_ads[i].cost = Money{saved[i]["cost"].get<int64_t>()};
    }
    city.tv_ad_rates(tv_ads);
  }

  if (j.contains("weekly_vendor_spending")) {
    const auto& spending = j["weekly_vendor_spending"];
    for (int player_idx = 0; player_idx < NUMBER_OF_PLAYERS &&
                             player_idx < static_cast<int>(spending.size());
         ++player_idx) {
      for (const auto& [vendor_key, cents] : spending[player_idx].items()) {
        city.restore_vendor_spending(
            core::PlayerId{player_idx},
            static_cast<uint8_t>(std::stoi(vendor_key)),
            Money{cents.get<int64_t>()});
      }
    }
  }
}

} // namespace save
