/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/supply/StockOrder.h"

#include <algorithm>
#include <vector>
#include "core/character/Player.h"
#include "core/city/CentralDepot.h"
#include "core/city/City.h"
#include "core/city/MayorOffice.h"
#include "core/finance/Ledger.h"
#include "core/finance/Money.h"
#include "core/food/Ingredient.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/supply/FoodSupplier.h"
#include "core/supply/IngredientBatch.h"
#include "core/supply/SupplierRelationship.h"
#include "engine/Logger.h"

namespace {

int total_items(const std::vector<IngredientBatch>& order) {
  int total = 0;
  for (const auto& batch : order) {
    total += batch.count;
  }
  return total;
}

void take_from_supplier(FoodSupplier* supplier, const IngredientBatch& batch) {
  if (supplier) {
    int remaining =
        std::max(0, supplier->stock(batch.ingredient_type) - batch.count);
    supplier->set_stock(batch.ingredient_type, remaining);
  }
}

// Record vendor spending for this week and update supplier relationship
void record_supplier_spend(City& city, const Player& player,
                           FoodSupplier* supplier, Money total_cost) {
  if (supplier && total_cost.cents > 0) {
    city.record_vendor_purchase(player.id(), supplier->id(), total_cost);
    supplier->relationship(player.id()).total_spent += total_cost;
  }
}

} // namespace

Money order_total(const std::vector<IngredientBatch>& order) {
  Money total{0};
  for (const auto& batch : order) {
    if (batch.count > 0) {
      total += Money{batch.price_per_unit.cents * batch.count};
    }
  }
  return total;
}

bool buy_for_restaurant(GameState& state, Player& player, City& city,
                        Restaurant& restaurant, FoodSupplier* supplier,
                        const std::vector<IngredientBatch>& order) {
  Money total_cost = order_total(order);
  int items = total_items(order);
  Logger::info("[Order] Total: {} items for {}", items, total_cost);

  if (total_cost.cents > 0 && !state.spend_cash(player, MonthlyCategory::Stock,
                                                total_cost, restaurant.id())) {
    return false;
  }
  city.mayor_office().add_investment_activity(player.id().value, total_cost);

  for (const auto& batch : order) {
    if (batch.count > 0) {
      Money cost =
          restaurant.purchase_ingredients(batch.ingredient_type, batch.count,
                                          batch.quality, batch.price_per_unit);
      restaurant.record_ingredient_spend(batch.ingredient_type, cost);
      take_from_supplier(supplier, batch);

      Logger::info("[Order] {} x{} @ {} each (quality {})",
                   batch.ingredient->name(), batch.count, batch.price_per_unit,
                   batch.quality);
    }
  }

  record_supplier_spend(city, player, supplier, total_cost);

  if (supplier && items > 0) {
    restaurant.preferred_suppliers()[supplier->category()] = supplier->id();
  }
  return true;
}

bool buy_for_depot(GameState& state, Player& player, City& city,
                   FoodSupplier* supplier,
                   const std::vector<IngredientBatch>& order) {
  Money total_cost = order_total(order);
  int items = total_items(order);
  Logger::info("[Order] Total: {} items for {}", items, total_cost);

  if (total_cost.cents > 0 &&
      !state.spend_cash(player, MonthlyCategory::Stock, total_cost)) {
    return false;
  }
  city.mayor_office().add_investment_activity(player.id().value, total_cost);

  auto& depot = city.central_depot(player.id());
  for (const auto& batch : order) {
    if (batch.count > 0) {
      depot.add_ingredient(batch.ingredient_type, batch.count * 1000,
                           batch.quality, batch.price_per_unit);
      take_from_supplier(supplier, batch);
      Logger::info("[Depot Order] {} x{} @ {} (quality {})",
                   batch.ingredient->name(), batch.count, batch.price_per_unit,
                   batch.quality);
    }
  }

  int social_gain = items / 1000;
  if (social_gain > 0) {
    state.adjust_social_score(player, social_gain);
  }

  record_supplier_spend(city, player, supplier, total_cost);
  return true;
}

void sell_from_depot(GameState& state, Player& player, City& city,
                     const std::vector<IngredientBatch>& order) {
  Money total_cost = order_total(order);
  Logger::info("[Order] Total: {} items for {}", total_items(order),
               total_cost);

  auto& depot = city.central_depot(player.id());
  Money book_value{0};
  for (const auto& batch : order) {
    if (batch.count > 0) {
      const auto& stock = depot.get_stock(batch.ingredient_type);
      book_value =
          book_value + Money{stock.weighted_avg_cost.cents * batch.count};
      depot.consume_ingredient(batch.ingredient_type, batch.count * 1000);
      Logger::info("[Depot Sale] {} x{} @ {} each", batch.ingredient->name(),
                   batch.count, batch.price_per_unit);
    }
  }

  // The stock account drops by the full book value; the difference with the
  // proceeds is a gain or loss on the sale.
  auto date = state.time().current_date();
  Money relieved{std::min(book_value.cents, total_cost.cents)};
  if (relieved.cents > 0) {
    player.financials().transfer(date, Account::Stock, Account::Cash, relieved,
                                 balance_sheet_only);
  }
  if (book_value.cents > total_cost.cents) {
    player.financials().transfer(date, Account::Stock, Account::ProfitAndLoss,
                                 Money{book_value.cents - total_cost.cents},
                                 balance_sheet_only);
  }
  else if (total_cost.cents > book_value.cents) {
    player.financials().transfer(date, Account::ProfitAndLoss, Account::Cash,
                                 Money{total_cost.cents - book_value.cents},
                                 balance_sheet_only);
  }
}

bool ship_between_depots(GameState& state, Player& player, City& from, City& to,
                         const std::vector<ShippedIngredient>& items,
                         Money transport_cost) {
  if (transport_cost.cents > 0 &&
      !state.spend_cash(player, MonthlyCategory::Stock, transport_cost)) {
    return false;
  }

  auto& source_depot = from.central_depot(player.id());
  auto& target_depot = to.central_depot(player.id());
  int shipped_kg = 0;

  for (const auto& item : items) {
    if (item.kg <= 0) {
      continue;
    }
    int qty_grams = item.kg * 1000;

    // Read source quality and cost before consuming
    const auto* stock =
        source_depot.get_ingredient_stock(static_cast<int>(item.type));
    int quality = stock ? stock->weighted_avg_quality : 100;
    Money cost = stock ? stock->weighted_avg_cost : Money{0};

    source_depot.consume_ingredient(item.type, qty_grams);
    target_depot.add_ingredient(item.type, qty_grams, quality, cost);
    shipped_kg += item.kg;
  }
  player.add_trade_activity(shipped_kg);
  state.adjust_social_score(player, shipped_kg / 1000);
  return true;
}
