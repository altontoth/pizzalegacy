/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/AIEconomy.h"

#include <algorithm>
#include <string>
#include <vector>

#include "core/character/AiMenuSlot.h"
#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/District.h"
#include "core/city/Property.h"
#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/item/ActionLine.h"
#include "core/item/ActionText.h"
#include "core/item/Item.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "engine/Logger.h"
#include "engine/Random.h"

namespace ai {

// Cash per tier the AI needs to have before increasing furniture count
constexpr Money furnishing_cash_per_tier = 1000_usd;

// An empty restaurant is furnished in one go rather than piece by piece.
constexpr int bare_restaurant = 10;

// The check runs before the increase, so the number can end up a bit higher.
constexpr int customers_per_day_ceiling = 500;

void invest_in_restaurant(Restaurant& restaurant, Player& player) {
  const auto& character = player.character();
  if (!engine::random::rand_percentage_chance(character.management())) {
    return;
  }
  // The money is just used as a condition
  const Money required_cash = furnishing_cash_per_tier * restaurant.ai_tier() *
                              character.courage() / 100;
  if (player.cash_balance() < required_cash) {
    return;
  }
  int step = engine::random::rand_max_n(5) + 1;
  if (restaurant.ai_customers_per_day() < customers_per_day_ceiling) {
    restaurant.ai_customers_per_day(restaurant.ai_customers_per_day() +
                                    engine::random::rand_max_n(40));
  }
  if (restaurant.ai_furniture_count() < bare_restaurant) {
    step = opening_furniture_count;
  }
  if (engine::random::rand_percentage_chance(restaurant.ai_furniture_count())) {
    return;
  }
  restaurant.ai_layout(restaurant.ai_layout_index(),
                       restaurant.ai_furniture_count() + step);
}

void process_restaurant_revenue(Restaurant& restaurant, Player& player) {
  // This is just +1 every day the restaurant is processed,
  // unconditionally (the original computes today's profit here but discards it
  // and always adds 1; profitability is not actually a factor). Sabotage is the
  // only thing that pushes it down. We might consider "fixing" this, but it
  // could have been a gameplay decision rather than a bug, so we'll have to
  // see what this would mean for how often restaurants get closed.
  restaurant.ai_increment_profit_days();

  const auto& counts = restaurant.economy().customer_counts();
  int total_customers = 0;
  for (auto ct : ALL_CONSUMER_TYPES) {
    total_customers += counts[ct];
  }

  if (total_customers <= 0) {
    return;
  }

  int per_customer = restaurant.ai_revenue_per_customer();
  int64_t revenue_cents =
      static_cast<int64_t>(per_customer) * total_customers * 100;

  int tier = restaurant.ai_tier();
  const auto& menu = restaurant.economy().menu_factor();
  int menu_sum = 0;
  for (auto ct : ALL_CONSUMER_TYPES) {
    menu_sum += menu[ct];
  }
  int avg_menu = menu_sum / static_cast<int>(ALL_CONSUMER_TYPES.size());
  int64_t cost_cents = (static_cast<int64_t>(tier) * 50 +
                        static_cast<int64_t>(avg_menu) / 20 * total_customers) *
                       100;

  // AI receipts never reach a ledger, so accumulate gross turnover here for the
  // competitor "turnover in $" chart (the human path books this in Restaurant).
  player.add_turnover(restaurant.property().city().id(), Money{revenue_cents});

  Logger::info("[AI] {} ({}) - Revenue: {}, Costs: {}, Customers: {}",
               player.name(), restaurant.name(), Money{revenue_cents},
               Money{cost_cents}, total_customers);
}

bool check_bankruptcy(Restaurant& restaurant, Player& player,
                      GameState& game_state) {
  // either bad profit OR crowded market triggers the closure check
  bool bad_profit = restaurant.ai_profit_days() < -50;
  bool crowded = restaurant.economy().competitor_count() > 3;
  if (!bad_profit && !crowded) {
    return false;
  }

  // Double courage-gate: both rolls must pass to actually close
  uint8_t courage = player.character().courage();
  if (!engine::random::rand_percentage_chance(courage)) {
    return false;
  }
  if (!engine::random::rand_percentage_chance(courage)) {
    return false;
  }

  auto& property = restaurant.property();
  std::string district_name = property.district().name();

  restaurant.close_for_business();
  game_state.real_estate().end_property_lease(property, player);

  Logger::info("[AI] {} closes a restaurant in {}", player.name(),
               district_name);
  game_state.status().post(StatusMsg{
      .payload = compat::format("{} closes down restaurant in {}.",
                                player.name(), district_name),
      .scope = StatusScope::City,
  });
  return true;
}

AttackSeverity attack_competitor(Restaurant& restaurant, Player& player,
                                 GameState& game_state) {
  if (restaurant.ai_attack_cooldown() != 0) {
    return AttackSeverity::None;
  }
  if (!engine::random::rand_percentage_chance(50)) {
    return AttackSeverity::None;
  }
  if (!engine::random::rand_percentage_chance(player.character().courage())) {
    return AttackSeverity::None;
  }

  const auto& economy = restaurant.economy();
  const auto city_id = restaurant.property().city().id();
  const auto strongest = economy.strongest_competitor();
  if (!strongest) {
    return AttackSeverity::None;
  }
  const Restaurant* target = game_state.restaurant(*strongest);
  if (!target || target->property().city().id() != city_id) {
    return AttackSeverity::None;
  }

  auto owner = target->property().owner_or_tenant();
  if (!owner) {
    return AttackSeverity::None;
  }
  Player* victim = game_state.player(*owner);
  if (!victim || !victim->is_human_controlled()) {
    return AttackSeverity::None;
  }

  // A restaurant losing badly enough does a real attack instead. The original's
  // attack needs the target's mafia protection timer at zero, which never
  // happens, so the victim only ever sees the guardian angels message. We don't
  // know if that's a bug or a change that was made late in development after
  // game testing, so we'll leave it like this for now.
  const int mode = economy.competition_score() > 100 ? 1 : 0;

  restaurant.ai_attack_cooldown(((100 - player.character().dexterity()) / 10) *
                                (mode + 2));

  if (mode == 1) {
    game_state.status().post(StatusMsg{
        .payload = compat::format("Your guardian angels from the underworld "
                                  "prevented an attack by {} on your "
                                  "restaurant {} last night!",
                                  player.name(), target->name()),
        .scope = StatusScope::City,
    });
    return AttackSeverity::Heavy;
  }

  game_state.adjust_underworld_score(
      player, (static_cast<int>(core::item::Item::StinkBomb) + 1) * 10);

  Logger::info("[AI] {} has a stink bomb set off at {}'s restaurant",
               player.name(), victim->name());
  // The original's text lookup is 1-based but is handed a 0-based item id, so
  // it names the item rather than saying what happened.
  const std::string line =
      game_state.options().fix_legacy_bugs
          ? core::item::ActionText::get(
                core::item::ActionLine::StinkBombAStinkBombHasExploded)
          : std::string{core::item::to_string(core::item::Item::StinkBomb)};

  // Held back until mid-morning.
  constexpr int not_before_block = 58;
  const int now = game_state.time().current_block_of_day();
  game_state.status().post(StatusMsg{
      .payload = compat::format("{}: {}", target->name(), line),
      .severity = StatusSeverity::Warning,
      .scope = StatusScope::City,
      .delay_blocks = std::max(0, not_before_block - now),
  });
  return AttackSeverity::Light;
}

namespace {

// True when the restaurant used up the day's one attempt in its city.
bool evaluate_restaurant(Restaurant& restaurant, Player& player,
                         GameState& game_state) {
  restaurant.ai_decrement_attack_cooldown();

  if (check_bankruptcy(restaurant, player, game_state)) {
    return true;
  }
  if (attack_competitor(restaurant, player, game_state) ==
      AttackSeverity::Heavy) {
    return true;
  }
  invest_in_restaurant(restaurant, player);
  return false;
}

} // namespace

void run_daily_evaluation(Player& player, GameState& game_state) {
  const auto scope = game_state.simulated_cities_for(player.id());

  // A closure ends the restaurant partway through the walk.
  std::vector<core::RestaurantId> ids;
  for (const auto* rest : game_state.restaurants_for(player.id())) {
    if (rest) {
      ids.push_back(rest->id());
    }
  }

  for (auto id : ids) {
    auto* rest = game_state.restaurant(id);
    if (!rest || !rest->wants_open() || rest->is_force_closed()) {
      continue;
    }
    const auto city = rest->property().city().id();
    if (std::ranges::find(scope, city) == scope.end() ||
        game_state.turn().ai_action_taken(city)) {
      continue;
    }
    if (evaluate_restaurant(*rest, player, game_state)) {
      game_state.turn().mark_ai_action(city);
    }
  }
}

namespace {

// One slot's random draw: a pizza off the canonical list, and a production
// ceiling the AI's cooking skill widens. Only the first 99 of the 100 pizza
// names are reachable.
void generate_menu_slot(AiMenuSlot& slot, uint8_t cooking) {
  slot.pizza_id = core::AiPizzaId{engine::random::rand_max_n(99) + 1};
  slot.production_counter = 0;
  slot.max_production = engine::random::rand_max_n(cooking) + 30;
}

} // namespace

void init_ai_menu(Player& player) {
  auto& slots = player.ai_menu_slots();
  uint8_t cooking = player.character().cooking();

  for (int i = 0; i < ai_menu_slot_count; ++i) {
    if (!engine::random::rand_percentage_chance(60)) {
      continue;
    }

    // The whole slot is redrawn until its pizza is one no earlier slot holds.
    bool duplicate;
    do {
      generate_menu_slot(slots[i], cooking);
      duplicate = false;
      for (int j = 0; j < i; ++j) {
        if (slots[j].pizza_id == slots[i].pizza_id) {
          duplicate = true;
          break;
        }
      }
    } while (duplicate);

    slots[i].production_counter =
        engine::random::rand_max_n(slots[i].max_production);
  }

  // Pre-warm the history buffer (4 passes, matching original)
  for (int w = 0; w < 4; ++w) {
    for (auto& slot : slots) {
      slot.update(1); // Initial restaurant count = 1
    }
  }

  Logger::info("[AI] Initialized menu for {}: {} active slots", player.name(),
               std::ranges::count_if(slots, [](const AiMenuSlot& s) {
                 return s.pizza_id != ai_pizza_none;
               }));
}

void update_ai_menu(Player& player, int restaurant_count) {
  for (auto& slot : player.ai_menu_slots()) {
    slot.update(restaurant_count);
  }
}

} // namespace ai
