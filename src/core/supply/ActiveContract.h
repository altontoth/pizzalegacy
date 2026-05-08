/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <vector>
#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/food/IngredientCategoryType.h"
#include "core/food/IngredientType.h"

struct ActiveContract {
  struct Item {
    core::food::IngredientType type;
    int kg_per_day;
  };

  uint8_t supplier_id{0};
  IngredientCategory category{};
  core::CityId city_id{-1};
  core::RestaurantId restaurant_id{-1};
  std::vector<Item> items;
  uint8_t discount_pct{0};
  uint8_t remaining_term{28};
  uint8_t original_term{28};
  Money daily_sum{0}; // total order before discount
  Money charge{0};    // service fee charged on as-required deliveries
  Money cancellation_penalty{0};
  bool deliver_as_required{false}; // top up to 2x daily demand beyond contract
};
