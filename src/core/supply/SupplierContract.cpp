/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/supply/SupplierContract.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>
#include "core/character/Player.h"
#include "core/finance/Money.h"
#include "core/supply/ActiveContract.h"
#include "core/supply/FoodSupplier.h"
#include "core/supply/SupplierRelationship.h"

SupplierContract opening_contract_terms(const FoodSupplier& supplier,
                                        const SupplierRelationship& rel,
                                        int spending_rank,
                                        Money total_order_value) {
  SupplierContract terms;
  terms.total_value = total_order_value;
  terms.delivery_days = std::max<uint8_t>(28, rel.last_delivery_days);

  if (rel.contract_count == 0) {
    terms.discount_score = 0;
    terms.penalty = total_order_value * 5;
  }
  else {
    // Original formula: (((order / min) + spending_rank) * 4 + past_discount)
    // / 3, max 60
    int64_t min_cents = supplier.minimum_contract_amount().cents;
    int ratio = (min_cents > 0)
                    ? static_cast<int>(total_order_value.cents / min_cents)
                    : 0;
    int raw = ((ratio + spending_rank) * 4) + rel.relationship_score;
    terms.discount_score = static_cast<uint8_t>(std::min(raw / 3, 60));
    terms.penalty = total_order_value * 3;
  }

  if (spending_rank > 3) {
    terms.charge = Money{0};
  }
  else {
    terms.charge = Money{static_cast<int64_t>(rel.frustration * 100)} +
                   total_order_value / 30;
  }

  return terms;
}

void sign_contract(Player& player, const FoodSupplier& supplier,
                   SupplierRelationship& rel, const SupplierContract& terms,
                   Money order_value, core::CityId city_id,
                   core::RestaurantId restaurant_id,
                   std::vector<ActiveContract::Item> items) {
  ActiveContract contract;
  contract.supplier_id = supplier.id();
  contract.category = supplier.category();
  contract.city_id = city_id;
  contract.restaurant_id = restaurant_id;
  contract.discount_pct = terms.discount_score;
  contract.remaining_term = terms.delivery_days;
  contract.original_term = terms.delivery_days;
  contract.daily_sum = terms.total_value;
  contract.charge = terms.charge;
  contract.cancellation_penalty = terms.penalty;
  contract.items = std::move(items);
  player.add_contract(std::move(contract));

  rel.contract_count++;
  rel.last_delivery_days = terms.delivery_days;
  rel.last_contract_sum = order_value;
  rel.relationship_score = terms.discount_score;
  if (rel.frustration > 0) {
    rel.frustration--;
  }
}
