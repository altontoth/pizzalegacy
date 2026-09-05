/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/save/SaveGameCharacter.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>
#include "core/character/Awards.h"
#include "core/character/InsurancePlayerState.h"
#include "core/character/Player.h"
#include "core/common/Ids.h"
#include "core/factories/CharacterFactory.h"
#include "core/food/IngredientCategoryType.h"
#include "core/marketing/NewspaperAdFormat.h"
#include "core/save/SaveGameDate.h"
#include "core/state/GameState.h"
#include "core/status/StatusHub.h"
#include "core/supply/ActiveContract.h"
#include "core/util/GameContext.h"

namespace save {

json character_to_json(const core::character::Character& character) {
  json j;
  j["id"] = character.id();
  // The name is the character's own: a player may rename theirs.
  j["first_name"] = character.first_name();
  j["last_name"] = character.last_name();
  // Save dynamic values that can change
  j["age"] = character.age();
  j["popularity"] = character.popularity();
  j["health"] = character.health();
  j["energy"] = character.energy();
  j["intelligence"] = character.intelligence();
  j["charisma"] = character.charisma();
  j["dexterity"] = character.dexterity();
  j["personality"] = character.personality();
  j["courage"] = character.courage();
  j["experience"] = character.experience();
  j["trustworthiness"] = character.trustworthiness();
  j["honesty"] = character.honesty();
  j["cooking"] = character.cooking();
  j["serving"] = character.serving();
  j["management"] = character.management();
  j["motivation"] = character.motivation();
  j["pizzas_per_day"] = character.pizzas_per_day();
  return j;
}

core::character::Character json_to_character(const json& j) {
  uint8_t char_id = j["id"].get<uint8_t>();
  auto* char_data = CharacterFactory::create_character(char_id);
  core::character::Character character(char_data);

  if (j.contains("first_name")) {
    character.name(j["first_name"].get<std::string>(),
                   j.value("last_name", std::string{}));
  }

  // Restore dynamic values
  if (j.contains("age")) {
    character.age(j["age"].get<uint8_t>());
  }
  if (j.contains("popularity")) {
    character.popularity(j["popularity"].get<uint8_t>());
  }
  if (j.contains("health")) {
    character.health(j["health"].get<uint8_t>());
  }
  if (j.contains("energy")) {
    character.energy(j["energy"].get<uint8_t>());
  }
  if (j.contains("intelligence")) {
    character.intelligence(j["intelligence"].get<uint8_t>());
  }
  if (j.contains("charisma")) {
    character.charisma(j["charisma"].get<uint8_t>());
  }
  if (j.contains("dexterity")) {
    character.dexterity(j["dexterity"].get<uint8_t>());
  }
  if (j.contains("personality")) {
    character.personality(j["personality"].get<uint8_t>());
  }
  if (j.contains("courage")) {
    character.courage(j["courage"].get<uint8_t>());
  }
  if (j.contains("experience")) {
    character.experience(j["experience"].get<uint8_t>());
  }
  if (j.contains("trustworthiness")) {
    character.trustworthiness(j["trustworthiness"].get<uint8_t>());
  }
  if (j.contains("honesty")) {
    character.honesty(j["honesty"].get<uint8_t>());
  }
  if (j.contains("cooking")) {
    character.cooking(j["cooking"].get<uint8_t>());
  }
  if (j.contains("serving")) {
    character.serving(j["serving"].get<uint8_t>());
  }
  if (j.contains("management")) {
    character.management(j["management"].get<uint8_t>());
  }
  if (j.contains("motivation")) {
    character.motivation(j["motivation"].get<uint8_t>());
  }
  if (j.contains("pizzas_per_day")) {
    character.pizzas_per_day(j["pizzas_per_day"].get<int>());
  }

  return character;
}

static json insurance_state_to_json(const InsurancePlayerState& state) {
  json j;
  json policies_array = json::array();
  for (const auto& policy : state.policies) {
    json p;
    p["actor_id"] = policy.actor_id.value;
    p["company_name"] = policy.company_name;
    p["monthly_premium"] = policy.monthly_premium.cents;
    p["max_coverage"] = policy.max_coverage.cents;
    policies_array.push_back(p);
  }
  j["policies"] = policies_array;
  if (state.pending_claim) {
    j["pending_claim"] = static_cast<int>(*state.pending_claim);
  }
  return j;
}

static InsurancePlayerState json_to_insurance_state(const json& j) {
  InsurancePlayerState state;
  if (j.contains("policies")) {
    int i = 0;
    for (const auto& p : j["policies"]) {
      if (i >= static_cast<int>(state.policies.size())) {
        break;
      }
      state.policies[i].actor_id = core::ActorId{p.value("actor_id", -1)};
      state.policies[i].company_name = p.value("company_name", "");
      state.policies[i].monthly_premium =
          Money{p.value("monthly_premium", int64_t{0})};
      state.policies[i].max_coverage =
          Money{p.value("max_coverage", int64_t{0})};
      ++i;
    }
  }
  if (j.contains("pending_claim") && !j["pending_claim"].is_null()) {
    state.pending_claim =
        static_cast<InsuranceType>(j["pending_claim"].get<int>());
  }
  return state;
}

json player_to_json(const Player& player) {
  json j;
  j["id"] = player.id().value;
  j["name"] = player.name();
  j["jail_time"] = player.jail_time();
  j["human_controlled"] = player.is_human_controlled();
  j["character"] = character_to_json(player.character());

  // Save recipes
  j["recipes"] = player_recipes_to_json(player.recipes());

  // Save financials (including ledger)
  j["financials"] = player_financials_to_json(player.financials());

  j["start_of_day_hour"] = player.start_of_day_hour();
  j["end_of_day_block"] = player.end_of_day_block();
  j["ai_expansion_cooldown"] = player.ai_expansion_cooldown();
  if (player.holiday_days_left().has_value()) {
    j["holiday_days_left"] = *player.holiday_days_left();
  }
  j["criminal_record"] = player.criminal_record();
  if (player.status() == PlayerStatus::Eliminated) {
    j["eliminated"] = true;
  }
  if (player.jail_days_left().has_value()) {
    j["jail_days_left"] = *player.jail_days_left();
  }

  // Save AI menu slots (only meaningful for AI players, but always written for
  // simplicity)
  json ai_menu_array = json::array();
  for (const auto& slot : player.ai_menu_slots()) {
    json slot_j;
    // Key spelling predates the field name.
    slot_j["recipe_id"] = slot.pizza_id.value;
    slot_j["production_counter"] = slot.production_counter;
    slot_j["max_production"] = slot.max_production;
    json history_array = json::array();
    for (int value : slot.history) {
      history_array.push_back(value);
    }
    slot_j["history"] = history_array;
    ai_menu_array.push_back(slot_j);
  }
  j["ai_menu_slots"] = ai_menu_array;

  // Save loans (map<CityId, vector<Loan>>)
  json loans_obj = json::object();
  for (const auto& [city_id, city_loans] : player.loans()) {
    json loans_array = json::array();
    for (const auto& loan : city_loans) {
      loans_array.push_back(loan_to_json(loan));
    }
    loans_obj[std::to_string(city_id.value)] = loans_array;
  }
  j["loans"] = loans_obj;

  // Save investments (array of up to 4 optional slots)
  json inv_array = json::array();
  for (const auto& slot : player.investments()) {
    if (slot.has_value()) {
      inv_array.push_back(investment_to_json(*slot));
    }
  }
  j["investments"] = inv_array;

  // Sorted, so the same game always writes the same document.
  std::vector<int> visited_cities;
  visited_cities.reserve(player.visited_city_ids().size());
  for (const auto& city_id : player.visited_city_ids()) {
    visited_cities.push_back(city_id.value);
  }
  std::ranges::sort(visited_cities);
  j["visited_city_ids"] = visited_cities;

  j["inventory"] = inventory_to_json(player.inventory());

  j["social_score"] = player.social_status().social_score();
  j["underworld_score"] = player.social_status().underworld_score();
  j["weapon_dealing_profit"] = player.weapon_dealing_profit().cents;
  json weapon_costs_obj = json::object();
  for (auto item : core::item::weapon_list) {
    Money unit = player.weapon_unit_cost(item);
    if (unit.cents > 0) {
      weapon_costs_obj[std::string(core::item::to_string(item))] = unit.cents;
    }
  }
  j["weapon_unit_costs"] = weapon_costs_obj;
  j["competition_willingness"] = player.competition_willingness();
  j["competition_win_count"] = player.competition_win_count();

  json ad_effectiveness_array = json::array();
  for (const auto& [city_id, channels] : player.ad_effectiveness()) {
    ad_effectiveness_array.push_back(
        json{{"city_id", city_id.value}, {"channels", channels}});
  }
  j["ad_effectiveness"] = ad_effectiveness_array;

  j["score"] = player.score();
  json awards_array = json::array();
  for (const auto& level : player.awards()) {
    awards_array.push_back(static_cast<int>(level));
  }
  j["awards"] = awards_array;

  j["mafia_state"] = mafia_state_to_json(player.mafia_state());
  j["insurance_state"] = insurance_state_to_json(player.insurance_state());

  // Save active supplier contracts
  json contracts_array = json::array();
  for (const auto& c : player.contracts()) {
    json cj;
    cj["supplier_id"] = c.supplier_id;
    cj["category"] = static_cast<int>(c.category);
    cj["city_id"] = c.city_id.value;
    cj["restaurant_id"] = c.restaurant_id.value;
    cj["discount_pct"] = c.discount_pct;
    cj["remaining_term"] = c.remaining_term;
    cj["original_term"] = c.original_term;
    cj["daily_sum"] = c.daily_sum.cents;
    cj["charge"] = c.charge.cents;
    cj["cancellation_penalty"] = c.cancellation_penalty.cents;
    cj["deliver_as_required"] = c.deliver_as_required;
    json items_array = json::array();
    for (const auto& item : c.items) {
      items_array.push_back({{"type", static_cast<int>(item.type)},
                             {"kg_per_day", item.kg_per_day}});
    }
    cj["items"] = items_array;
    contracts_array.push_back(cj);
  }
  j["contracts"] = contracts_array;

  json pending_notifications_array = json::array();
  for (const auto& msg : player.pending_notifications()) {
    if (const auto* text = std::get_if<std::string>(&msg.payload)) {
      json mj;
      mj["text"] = *text;
      mj["severity"] = static_cast<int>(msg.severity);
      mj["scope"] = static_cast<int>(msg.scope);
      pending_notifications_array.push_back(mj);
    }
  }
  j["pending_notifications"] = pending_notifications_array;

  // Save active advertising campaigns.
  json campaigns_array = json::array();
  for (const auto& [city_id, slots] : player.get_campaigns()) {
    for (const auto& campaign_opt : slots) {
      if (!campaign_opt.has_value()) {
        continue;
      }
      const auto& c = *campaign_opt;
      json cj;
      cj["channel"] = static_cast<int>(c.channel);
      cj["city_id"] = c.city_id.value;
      cj["days_remaining"] = c.days_remaining;
      cj["effectiveness"] = c.rate.effectiveness;
      cj["cost"] = c.rate.cost.cents;
      if (c.flyer_count) {
        cj["flyer_count"] = *c.flyer_count;
      }
      if (c.poster_id) {
        cj["poster_id"] = *c.poster_id;
      }
      if (c.newspaper_ad_format) {
        cj["newspaper_ad_format"] = static_cast<int>(*c.newspaper_ad_format);
      }
      if (!c.newspaper_lines.empty()) {
        cj["newspaper_lines"] = c.newspaper_lines;
      }
      if (c.tv_ad_id) {
        cj["tv_ad_id"] = *c.tv_ad_id;
      }
      campaigns_array.push_back(cj);
    }
  }
  j["campaigns"] = campaigns_array;

  // Per-city gross turnover accumulator (backs the competitor turnover chart;
  // the ledger only tracks the human, so this must persist for all players).
  json turnover_array = json::array();
  for (int c = 0; c < NUMBER_OF_CITIES; ++c) {
    turnover_array.push_back(player.turnover_in_city(core::CityId{c}).cents);
  }
  j["turnover_by_city"] = turnover_array;

  return j;
}

std::unique_ptr<Player>
json_to_player(const json& j, GameContext& context,
               const core::food::CustomerTasteProfiles& taste_profiles) {
  core::PlayerId pid{j["id"].get<int>()};
  bool human{j["human_controlled"].get<bool>()};
  auto character = json_to_character(j["character"]);

  auto player = std::make_unique<Player>(pid, character, human);
  player->name(j["name"].get<std::string>());
  player->jail_time(j["jail_time"].get<uint8_t>());

  // Restore recipes
  if (j.contains("recipes")) {
    player->recipes() = json_to_player_recipes(j["recipes"], taste_profiles);
  }

  // Restore financials (including ledger)
  if (j.contains("financials")) {
    player->financials() = json_to_player_financials(j["financials"]);
  }

  if (j.contains("ai_expansion_cooldown")) {
    player->ai_expansion_cooldown(j["ai_expansion_cooldown"].get<int>());
  }
  if (j.contains("start_of_day_hour")) {
    player->start_of_day_hour(j["start_of_day_hour"].get<uint8_t>());
  }
  if (j.contains("end_of_day_block")) {
    player->end_of_day_block(j["end_of_day_block"].get<int>());
  }
  if (j.contains("holiday_days_left")) {
    player->holiday(j["holiday_days_left"].get<int>());
  }
  if (j.contains("criminal_record")) {
    int cr = j["criminal_record"].get<int>();
    for (int i = 0; i < cr; ++i) {
      player->increment_criminal_record();
    }
  }
  if (j.contains("eliminated") && j["eliminated"].get<bool>()) {
    player->eliminate();
  }
  if (j.contains("jail_days_left")) {
    player->jail(j["jail_days_left"].get<int>());
  }

  // Restore AI menu slots
  if (j.contains("ai_menu_slots")) {
    auto& slots = player->ai_menu_slots();
    int i = 0;
    for (const auto& slot_j : j["ai_menu_slots"]) {
      if (i >= static_cast<int>(slots.size())) {
        break;
      }
      slots[i].pizza_id = core::AiPizzaId{slot_j.value("recipe_id", 0)};
      slots[i].production_counter = slot_j.value("production_counter", 0);
      slots[i].max_production = slot_j.value("max_production", 0);
      if (slot_j.contains("history")) {
        int hi = 0;
        for (const auto& val_j : slot_j["history"]) {
          if (hi >= static_cast<int>(slots[i].history.size())) {
            break;
          }
          slots[i].history[hi++] = val_j.get<int>();
        }
      }
      ++i;
    }
  }

  // Restore loans
  if (j.contains("loans")) {
    for (auto it = j["loans"].begin(); it != j["loans"].end(); ++it) {
      core::CityId city_id{std::stoi(it.key())};
      for (const auto& loan_j : it.value()) {
        Loan loan = json_to_loan(loan_j);
        player->loans_in_city(city_id).push_back(loan);
      }
    }
  }

  // Restore investments
  if (j.contains("investments")) {
    for (const auto& inv_j : j["investments"]) {
      player->add_investment(json_to_investment(inv_j));
    }
  }

  // Restore inventory into existing object (Inventory is not copy-assignable)
  if (j.contains("inventory")) {
    load_inventory(j["inventory"], player->inventory());
  }
  if (j.contains("social_score")) {
    player->social_status().social_score(j["social_score"]);
  }
  if (j.contains("underworld_score")) {
    player->social_status().underworld_score(j["underworld_score"]);
  }
  player->weapon_dealing_profit(
      Money{j.value("weapon_dealing_profit", int64_t{0})});
  if (j.contains("weapon_unit_costs")) {
    const auto& costs = j["weapon_unit_costs"];
    for (auto item : core::item::weapon_list) {
      auto key = std::string(core::item::to_string(item));
      if (costs.contains(key)) {
        player->weapon_unit_cost(item, Money{costs[key].get<int64_t>()});
      }
    }
  }
  player->set_competition_willingness(
      j.value("competition_willingness", uint8_t{0}));
  player->competition_win_count(j.value("competition_win_count", uint8_t{0}));

  if (j.contains("ad_effectiveness")) {
    for (const auto& entry : j["ad_effectiveness"]) {
      if (!entry.contains("city_id") || !entry.contains("channels")) {
        continue;
      }
      player->ad_effectiveness(core::CityId{entry["city_id"].get<int>()},
                               entry["channels"].get<std::array<uint8_t, 4>>());
    }
  }

  if (j.contains("score")) {
    player->add_score(j["score"].get<int>());
  }
  if (j.contains("awards")) {
    int category = 0;
    for (const auto& level : j["awards"]) {
      if (category >= static_cast<int>(AwardCategory::Count)) {
        break;
      }
      player->award(static_cast<AwardCategory>(category),
                    static_cast<AwardLevel>(level.get<int>()));
      ++category;
    }
  }
  if (j.contains("mafia_state")) {
    player->mafia_state() = json_to_mafia_state(j["mafia_state"]);
  }
  if (j.contains("insurance_state")) {
    player->insurance_state() = json_to_insurance_state(j["insurance_state"]);
  }

  if (j.contains("visited_city_ids")) {
    for (const auto& val : j["visited_city_ids"]) {
      player->mark_city_visited(core::CityId{val.get<int>()});
    }
  }

  // Restore active supplier contracts
  if (j.contains("contracts")) {
    for (const auto& cj : j["contracts"]) {
      ActiveContract c;
      c.supplier_id = cj.value("supplier_id", uint8_t{0});
      c.category = static_cast<IngredientCategory>(cj.value("category", 0));
      c.city_id = core::CityId{cj.value("city_id", -1)};
      c.restaurant_id = core::RestaurantId{cj.value("restaurant_id", -1)};
      c.discount_pct = cj.value("discount_pct", uint8_t{0});
      c.remaining_term = cj.value("remaining_term", uint8_t{0});
      c.original_term = cj.value("original_term", uint8_t{0});
      c.daily_sum = Money{cj.value("daily_sum", int64_t{0})};
      c.charge = Money{cj.value("charge", int64_t{0})};
      c.cancellation_penalty =
          Money{cj.value("cancellation_penalty", int64_t{0})};
      c.deliver_as_required = cj.value("deliver_as_required", false);
      if (cj.contains("items")) {
        for (const auto& ij : cj["items"]) {
          c.items.push_back(
              {static_cast<core::food::IngredientType>(ij.value("type", 0)),
               ij.value("kg_per_day", 0)});
        }
      }
      if (c.remaining_term > 0) {
        player->add_contract(std::move(c));
      }
    }
  }

  if (j.contains("pending_notifications")) {
    for (const auto& mj : j["pending_notifications"]) {
      StatusMsg msg;
      msg.payload = mj.value("text", std::string{});
      msg.severity = static_cast<StatusSeverity>(mj.value("severity", 0));
      msg.scope = static_cast<StatusScope>(mj.value("scope", 0));
      player->queue_notification(std::move(msg));
    }
  }

  // Restore active advertising campaigns
  if (j.contains("campaigns")) {
    for (const auto& cj : j["campaigns"]) {
      AdvertisingCampaign c;
      c.channel = static_cast<AdvertisingChannel>(cj.value("channel", 0));
      c.city_id = core::CityId{cj.value("city_id", -1)};
      c.days_remaining = cj.value("days_remaining", 0);
      c.rate.effectiveness = cj.value("effectiveness", 0);
      c.rate.cost = Money{cj.value("cost", int64_t{0})};
      if (cj.contains("flyer_count")) {
        c.flyer_count = cj["flyer_count"].get<int>();
      }
      if (cj.contains("poster_id")) {
        c.poster_id = cj["poster_id"].get<int>();
      }
      if (cj.contains("newspaper_ad_format")) {
        c.newspaper_ad_format = static_cast<NewspaperAdFormat>(
            cj["newspaper_ad_format"].get<int>());
      }
      if (cj.contains("newspaper_lines")) {
        c.newspaper_lines =
            cj["newspaper_lines"].get<std::vector<std::string>>();
      }
      if (cj.contains("tv_ad_id")) {
        c.tv_ad_id = cj["tv_ad_id"].get<int>();
      }
      player->add_campaign(c);
    }
  }

  if (j.contains("turnover_by_city")) {
    const auto& arr = j["turnover_by_city"];
    for (int c = 0; c < NUMBER_OF_CITIES && c < static_cast<int>(arr.size());
         ++c) {
      player->set_turnover_in_city(core::CityId{c},
                                   Money{arr[c].get<int64_t>()});
    }
  }

  return player;
}

static json mission_location_to_json(const MissionLocation& loc) {
  json j;
  j["description"] = loc.description;
  j["tile_index"] = loc.tile_index;
  j["date"] = date_to_json(loc.date);
  j["hour"] = loc.hour;
  j["minute"] = loc.minute;
  return j;
}

static MissionLocation json_to_mission_location(const json& j) {
  MissionLocation loc;
  loc.description = j.value("description", "");
  loc.tile_index = j.value("tile_index", -1);
  if (j.contains("date")) {
    loc.date = json_to_date(j["date"]);
  }
  loc.hour = j.value("hour", 0);
  loc.minute = j.value("minute", 0);
  return loc;
}

static json mission_to_json(const MafiaMission& m) {
  json j;
  j["mission_type"] = static_cast<int>(m.mission_type);
  j["active"] = m.active;
  j["risk_pct"] = m.risk_pct;
  j["reward"] = m.reward.cents;
  j["deadline"] = date_to_json(m.deadline);
  j["deadline_hour"] = m.deadline_hour;
  j["deadline_minute"] = m.deadline_minute;
  j["completed"] = m.completed;
  j["busted"] = m.busted;
  j["item_index"] = m.item_index;
  j["quantity"] = m.quantity;
  j["clan"] = m.clan;
  j["city"] = m.city.value;
  j["cash_amount"] = m.cash_amount.cents;
  j["pickup"] = mission_location_to_json(m.pickup);
  j["dropoff"] = mission_location_to_json(m.dropoff);

  json banks = json::array();
  json launder = json::array();
  for (int i = 0; i < 4; ++i) {
    banks.push_back(m.bank_accounts[i]);
    launder.push_back(m.launder_done[i]);
  }
  j["bank_accounts"] = banks;
  j["launder_done"] = launder;
  j["contamination_pct"] = m.contamination_pct;
  j["profit"] = m.profit.cents;
  return j;
}

static MafiaMission json_to_mission(const json& j) {
  MafiaMission m;
  m.mission_type = static_cast<MafiaMissionType>(j.value("mission_type", 0));
  m.active = j.value("active", false);
  m.risk_pct = j.value("risk_pct", 0);
  m.reward = Money{j.value("reward", int64_t{0})};
  if (j.contains("deadline")) {
    m.deadline = json_to_date(j["deadline"]);
  }
  m.deadline_hour = j.value("deadline_hour", 0);
  m.deadline_minute = j.value("deadline_minute", 0);
  m.completed = j.value("completed", false);
  m.busted = j.value("busted", false);
  m.item_index = j.value("item_index", 0);
  m.quantity = j.value("quantity", 0);
  m.clan = j.value("clan", 0);
  m.city = core::CityId{j.value("city", 0)};
  m.cash_amount = Money{j.value("cash_amount", int64_t{0})};
  if (j.contains("pickup")) {
    m.pickup = json_to_mission_location(j["pickup"]);
  }
  if (j.contains("dropoff")) {
    m.dropoff = json_to_mission_location(j["dropoff"]);
  }
  if (j.contains("bank_accounts")) {
    for (int i = 0; i < 4 && i < static_cast<int>(j["bank_accounts"].size());
         ++i) {
      m.bank_accounts[i] = j["bank_accounts"][i].get<int>();
    }
  }
  if (j.contains("launder_done")) {
    for (int i = 0; i < 4 && i < static_cast<int>(j["launder_done"].size());
         ++i) {
      m.launder_done[i] = j["launder_done"][i].get<bool>();
    }
  }
  m.contamination_pct = j.value("contamination_pct", 0);
  m.profit = Money{j.value("profit", int64_t{0})};
  return m;
}

static json mafia_loan_to_json(const MafiaLoan& loan) {
  json j;
  j["amount"] = loan.amount.cents;
  j["due_date"] = date_to_json(loan.due_date);
  j["clan"] = loan.clan;
  return j;
}

static MafiaLoan json_to_mafia_loan(const json& j) {
  MafiaLoan loan;
  loan.amount = Money{j.value("amount", int64_t{0})};
  if (j.contains("due_date")) {
    loan.due_date = json_to_date(j["due_date"]);
  }
  loan.clan = j.value("clan", 0);
  return loan;
}

json mafia_state_to_json(const MafiaPlayerState& state) {
  json j;
  if (state.active_mission) {
    j["active_mission"] = mission_to_json(*state.active_mission);
  }
  if (state.loan) {
    j["loan"] = mafia_loan_to_json(*state.loan);
  }
  return j;
}

MafiaPlayerState json_to_mafia_state(const json& j) {
  MafiaPlayerState state;
  if (j.contains("active_mission") && !j["active_mission"].is_null()) {
    state.active_mission = json_to_mission(j["active_mission"]);
  }
  if (j.contains("loan") && !j["loan"].is_null()) {
    state.loan = json_to_mafia_loan(j["loan"]);
  }
  return state;
}

json player_recipes_to_json(const core::food::PlayerRecipes& recipes) {
  using namespace core::food;

  json j;
  j["next_id"] = recipes.next_id.value;

  // Save canonical recipes
  json canonical_array = json::array();
  for (size_t i = 0; i < recipes.canon.size(); ++i) {
    const auto& slot = recipes.canon[i];
    if (slot.created()) {
      canonical_array.push_back(recipe_to_json(*slot.recipe));
    }
    else {
      canonical_array.push_back(nullptr);
    }
  }
  j["canonical"] = canonical_array;

  // Save user recipes
  json user_array = json::array();
  for (const auto& recipe : recipes.user) {
    user_array.push_back(recipe_to_json(recipe));
  }
  j["user"] = user_array;

  return j;
}

core::food::PlayerRecipes json_to_player_recipes(
    const json& j, const core::food::CustomerTasteProfiles& taste_profiles) {
  using namespace core::food;

  PlayerRecipes recipes;
  recipes.next_id = core::RecipeId{j["next_id"].get<int>()};

  // Restore canonical recipes
  if (j.contains("canonical")) {
    for (size_t i = 0; i < j["canonical"].size() && i < recipes.canon.size();
         ++i) {
      if (!j["canonical"][i].is_null()) {
        auto recipe = json_to_recipe(j["canonical"][i], taste_profiles);
        recipes.canon[i] = CanonicalSlot{std::move(recipe)};
      }
    }
  }

  // Restore user recipes
  if (j.contains("user")) {
    for (const auto& recipe_json : j["user"]) {
      recipes.user.push_back(json_to_recipe(recipe_json, taste_profiles));
    }
  }

  return recipes;
}

json inventory_to_json(const core::inventory::Inventory& inventory) {
  json j = json::object();
  for (std::size_t i = 0; i < core::item::item_count; ++i) {
    auto item = static_cast<core::item::Item>(i);
    int qty = inventory[item];
    if (qty > 0) {
      j[std::string(core::item::to_string(item))] = qty;
    }
  }
  return j;
}

void load_inventory(const json& j, core::inventory::Inventory& inventory) {
  for (std::size_t i = 0; i < core::item::item_count; ++i) {
    auto item = static_cast<core::item::Item>(i);
    auto key = std::string(core::item::to_string(item));
    if (!j.contains(key)) {
      continue;
    }
    const auto& entry = j[key];
    // Save format version 3 wrote a per-item cost beside the count; the player
    // owns the price now, so only the count is read.
    inventory[item] =
        entry.is_object() ? entry.value("qty", 0) : entry.get<int>();
  }
}

json loan_to_json(const Loan& loan) {
  json j;
  j["bank_id"] = loan.bank_id().value;
  j["start_amount"] = loan.start_amount().cents;
  j["interest_percentage"] = loan.interest_percentage();
  j["start_months"] = loan.start_months();
  j["months_left"] = loan.months_left();
  j["monthly_principal"] = loan.monthly_principal().cents;
  j["monthly_interest"] = loan.monthly_interest().cents;
  return j;
}

Loan json_to_loan(const json& j) {
  return Loan::from_save(
      core::BankId{j["bank_id"].get<int32_t>()},
      Money{j["start_amount"].get<int64_t>()},
      j["interest_percentage"].get<uint8_t>(), j["start_months"].get<int>(),
      j["months_left"].get<int>(), Money{j["monthly_principal"].get<int64_t>()},
      Money{j["monthly_interest"].get<int64_t>()});
}

json investment_to_json(const Investment& inv) {
  json j;
  j["bank_id"] = inv.bank_id().value;
  j["city_id"] = inv.city_id().value;
  j["amount"] = inv.amount().cents;
  j["return_rate"] = inv.return_rate();
  j["start_months"] = inv.start_months();
  j["months_left"] = inv.months_left();
  j["monthly_return"] = inv.monthly_return().cents;
  return j;
}

Investment json_to_investment(const json& j) {
  return Investment::from_save(
      core::BankId{j["bank_id"].get<int32_t>()},
      core::CityId{j["city_id"].get<int32_t>()},
      Money{j["amount"].get<int64_t>()}, j["return_rate"].get<int>(),
      j["start_months"].get<int>(), j["months_left"].get<int>(),
      Money{j["monthly_return"].get<int64_t>()});
}

json posting_to_json(const Posting& posting) {
  json j;
  j["account"] = static_cast<int>(posting.account);
  j["amount"] = posting.amount.cents;
  if (posting.restaurant_id.has_value()) {
    j["restaurant_id"] = posting.restaurant_id.value().value;
  }
  else {
    j["restaurant_id"] = nullptr;
  }
  j["scope"] = static_cast<int>(posting.scope);
  return j;
}

Posting json_to_posting(const json& j) {
  Account account = static_cast<Account>(j["account"].get<int>());
  Money amount{j["amount"].get<int64_t>()};
  std::optional<core::RestaurantId> restaurant_id = std::nullopt;

  if (j.contains("restaurant_id") && !j["restaurant_id"].is_null()) {
    restaurant_id = core::RestaurantId{j["restaurant_id"].get<int32_t>()};
  }

  AccountingScope scope = AccountingScope::Operational;
  if (j.contains("scope")) {
    scope = static_cast<AccountingScope>(j["scope"].get<int>());
  }

  return Posting{account, amount, restaurant_id, scope};
}

json journal_entry_to_json(const JournalEntry& entry) {
  json j;

  // Save date
  j["date"] = {{"year", entry.date.year()},
               {"month", entry.date.month()},
               {"day", entry.date.day()}};

  // Save postings
  json postings_array = json::array();
  for (const auto& posting : entry.lines) {
    postings_array.push_back(posting_to_json(posting));
  }
  j["lines"] = postings_array;

  return j;
}

JournalEntry json_to_journal_entry(const json& j) {
  // Initialize with default date
  GameDate date{1995, 1, 1};

  // Restore date
  if (j.contains("date")) {
    int year = j["date"]["year"].get<int>();
    unsigned month = j["date"]["month"].get<unsigned>();
    unsigned day = j["date"]["day"].get<unsigned>();
    date = GameDate{year, static_cast<int>(month), static_cast<int>(day)};
  }

  JournalEntry entry{date, {}};

  // Restore postings
  if (j.contains("lines")) {
    for (const auto& posting_json : j["lines"]) {
      entry.lines.push_back(json_to_posting(posting_json));
    }
  }

  return entry;
}

json ledger_to_json(const Ledger& ledger) {
  json entries_array = json::array();
  for (const auto& entry : ledger.entries()) {
    entries_array.push_back(journal_entry_to_json(entry));
  }
  return entries_array;
}

Ledger json_to_ledger(const json& j) {
  Ledger ledger;
  for (const auto& entry_json : j) {
    ledger.post(json_to_journal_entry(entry_json));
  }
  return ledger;
}

json player_financials_to_json(const PlayerFinancials& financials) {
  json j;
  j["journal"] = ledger_to_json(financials.ledger());
  j["daily_profit_history"] = financials.daily_profit_history();
  j["daily_profit_days"] = financials.daily_profit_days();
  return j;
}

PlayerFinancials json_to_player_financials(const json& j) {
  PlayerFinancials financials;

  if (j.contains("journal")) {
    financials.restore_ledger(json_to_ledger(j["journal"]));
  }

  if (j.contains("daily_profit_history")) {
    std::array<int64_t, PlayerFinancials::daily_profit_days_max> history{};
    const auto& arr = j["daily_profit_history"];
    for (int i = 0; i < PlayerFinancials::daily_profit_days_max &&
                    i < static_cast<int>(arr.size());
         ++i) {
      history[i] = arr[i].get<int64_t>();
    }
    financials.set_daily_profit_history(history,
                                        j.value("daily_profit_days", 0));
  }

  return financials;
}

} // namespace save
