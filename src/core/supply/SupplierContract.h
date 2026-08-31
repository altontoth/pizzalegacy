/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <vector>
#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/supply/ActiveContract.h"

class FoodSupplier;
class Player;
struct SupplierRelationship;

struct SupplierContract {
  uint8_t discount_score{0}; // 0-60, discount percentage
  uint8_t delivery_days{28}; // Delivery period in days
  Money charge{0};           // Service charge/fee
  Money penalty{0};          // Late delivery penalty amount
  Money total_value{0};      // Total contract value (sum)
};

// The terms a supplier opens with, before any haggling.
SupplierContract opening_contract_terms(const FoodSupplier& supplier,
                                        const SupplierRelationship& rel,
                                        int spending_rank,
                                        Money total_order_value);

// order_value is the order as first quoted, before any haggling moved it.
void sign_contract(Player& player, const FoodSupplier& supplier,
                   SupplierRelationship& rel, const SupplierContract& terms,
                   Money order_value, core::CityId city_id,
                   core::RestaurantId restaurant_id,
                   std::vector<ActiveContract::Item> items);
