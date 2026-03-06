/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/finance/Money.h"

namespace core::restaurant {

/**
 * Represents a stock of a single ingredient type with weighted-average
 * quality and cost tracking.
 *
 * When new batches are added, quality and cost are averaged based on
 * quantities (weighted average), like in the original
 */
struct IngredientStock {
  int quantity{0};             // Grams in stock
  int weighted_avg_quality{0}; // Quality 0-100 (weighted average)
  Money weighted_avg_cost{0};  // Cost per kilogram (weighted average)
  Money last_price{0};

  void add_stock(int qty_grams, int quality, Money cost_per_kg) {
    if (qty_grams <= 0) {
      return;
    }

    if (quantity == 0) {
      // First batch - just set values
      quantity = qty_grams;
      weighted_avg_quality = quality;
      weighted_avg_cost = cost_per_kg;
    }
    else {
      // Weighted average calculation
      int total_qty = quantity + qty_grams;

      // Quality: weighted average
      weighted_avg_quality =
          (quantity * weighted_avg_quality + qty_grams * quality) / total_qty;

      // Cost: weighted average
      Money total_cost = Money{weighted_avg_cost.cents * quantity +
                               cost_per_kg.cents * qty_grams};
      weighted_avg_cost = Money{total_cost.cents / total_qty};

      quantity = total_qty;
    }
    last_price = cost_per_kg;
  }

  bool consume(int qty_grams) {
    if (qty_grams > quantity) {
      return false;
    }
    quantity -= qty_grams;
    return true;
  }

  /**
   * Apply daily quality degradation.
   * Default rate: 0.95 (5% degradation per day)
   */
  void apply_degradation(float degradation_rate = 0.95f) {
    weighted_avg_quality =
        std::max(static_cast<int>(weighted_avg_quality * degradation_rate), 0);
  }

  bool has_available(int required_grams) const {
    return quantity >= required_grams;
  }

  Money total_value() const {
    // Convert grams to kilograms
    return Money{(weighted_avg_cost.cents * quantity) / 1000};
  }

  int days_remaining(int daily_usage_grams) const {
    if (daily_usage_grams <= 0) {
      return -1; // Effectively infinite
    }
    // quantity is already in grams, just divide by daily usage
    return quantity / daily_usage_grams;
  }
};

} // namespace core::restaurant
