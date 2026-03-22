/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/AIEconomy.h"

#include "core/character/AiMenuSlot.h"
#include "core/character/Player.h"
#include "core/city/District.h"
#include "core/city/Property.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "engine/Logger.h"

namespace ai {

void process_restaurant_revenue(Restaurant& restaurant, Player& player,
                                const GameDate& date) {
  const auto& counts = restaurant.economy().customer_counts();
  int total_customers = 0;
  for (auto ct : ALL_CONSUMER_TYPES) {
    total_customers += counts[ct];
  }

  if (total_customers <= 0) {
    return;
  }

  int capacity_target = restaurant.ai_capacity_target();
  int64_t revenue_cents =
      static_cast<int64_t>(capacity_target) * total_customers * 100;

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

  if (revenue_cents > 0) {
    player.financials().transfer(date, Account::Sales, Account::Cash,
                                 Money{revenue_cents}, restaurant.id(),
                                 AccountingScope::Operational);
  }

  if (cost_cents > 0) {
    player.financials().transfer(date, Account::Cash, Account::Stock,
                                 Money{cost_cents}, restaurant.id(),
                                 AccountingScope::Operational);
  }

  restaurant.ai_increment_profit_days();

  Logger::info("[AI] {} ({}) - Revenue: {}, Costs: {}, Customers: {}",
               player.name(), restaurant.name(), Money{revenue_cents},
               Money{cost_cents}, total_customers);
}

bool check_bankruptcy(Restaurant& restaurant, Player& player,
                      GameState& game_state) {
  if (restaurant.ai_profit_days() >= -50) {
    return false;
  }
  if (restaurant.economy().competitor_count() <= 3) {
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

  auto* property = restaurant.property();
  std::string district_name =
      property ? property->district().name() : "unknown";

  restaurant.wants_open(false);
  if (property) {
    game_state.end_property_lease(*property, player);
  }

  Logger::info("[AI] {} closes a restaurant in {}", player.name(),
               district_name);
  game_state.status().post(StatusMsg{
      .payload = compat::format("{} closes down restaurant in {}.",
                                player.name(), district_name),
      .scope = StatusScope::City,
      .type = TransientType::Ephemeral,
  });
  return true;
}

void init_ai_menu(Player& player) {
  auto& slots = player.ai_menu_slots();
  uint8_t cooking = player.character().cooking();

  for (int i = 0; i < 10; ++i) {
    if (!engine::random::rand_percentage_chance(60)) {
      continue;
    }

    // Pick a random recipe_id 1-100, deduplicate against earlier slots
    uint8_t rid;
    bool duplicate;
    do {
      rid = static_cast<uint8_t>(engine::random::rand_max_n(100) + 1);
      duplicate = false;
      for (int j = 0; j < i; ++j) {
        if (slots[j].recipe_id == rid) {
          duplicate = true;
          break;
        }
      }
    } while (duplicate);

    slots[i].recipe_id = rid;
    slots[i].max_production = engine::random::rand_max_n(cooking) + 30;
    slots[i].production_counter =
        engine::random::rand_max_n(slots[i].max_production);
  }

  // Pre-warm the history buffer (4 passes, matching original)
  for (int w = 0; w < 4; ++w) {
    for (auto& slot : slots) {
      if (slot.recipe_id != 0) {
        slot.update(1); // Initial restaurant count = 1
      }
    }
  }

  Logger::info(
      "[AI] Initialized menu for {}: {} active slots", player.name(),
      std::count_if(slots.begin(), slots.end(),
                    [](const AiMenuSlot& s) { return s.recipe_id != 0; }));
}

void update_ai_menu(Player& player, int restaurant_count) {
  for (auto& slot : player.ai_menu_slots()) {
    if (slot.recipe_id != 0) {
      slot.update(restaurant_count);
    }
  }
}

} // namespace ai
