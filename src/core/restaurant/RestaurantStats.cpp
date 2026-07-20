/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/RestaurantStats.h"

#include <vector>
#include "core/finance/Money.h"
#include "core/time/GameDate.h"

using namespace core::restaurant;

// Archive today's stats to history and reset for new day.
void RestaurantStats::end_of_day(int total_capacity) {
  today.finalize(total_capacity);

  // Update all-time totals
  total_pizzas_sold += today.orders_fulfilled;
  total_ingredients_used_grams += today.ingredients_used_grams;
  total_revenue = total_revenue + today.revenue;
  total_profit = total_profit + today.net_profit;

  // Shift days forward
  day_before_yesterday = yesterday;
  yesterday = today;

  // Archive yesterday:
  history.push_back(yesterday);

  today.reset();
}

// Get the last N days of history (for graphs and reports).
std::vector<DailyStats> RestaurantStats::last_n_days(int n) const {
  if (n <= 0) {
    return {};
  }

  size_t available = history.size();
  if (available == 0) {
    return {};
  }

  size_t start_idx = (available > static_cast<size_t>(n)) ? available - n : 0;

  return std::vector<DailyStats>(history.begin() + start_idx, history.end());
}

std::vector<DailyStats> RestaurantStats::last_week() const {
  return last_n_days(7);
}
std::vector<DailyStats> RestaurantStats::last_month() const {
  return last_n_days(30);
}

std::vector<DailyStats>
RestaurantStats::current_week(const GameDate& current_date) const {
  std::vector<DailyStats> result;

  int days_into_week = current_date.week_day_index();

  if (days_into_week > 0 && !history.empty()) {
    size_t history_size = history.size();
    size_t start_idx = (history_size > static_cast<size_t>(days_into_week))
                           ? history_size - days_into_week
                           : 0;
    result.insert(result.end(), history.begin() + start_idx, history.end());
  }

  // Add today's stats
  result.push_back(today);

  return result;
}

int RestaurantStats::avg_daily_sales(int days) const {
  auto recent = last_n_days(days);
  if (recent.empty()) {
    return 0;
  }

  int total = 0;
  for (const auto& day : recent) {
    total += day.orders_fulfilled;
  }
  return total / static_cast<int>(recent.size());
}

int RestaurantStats::profit_margin_percent() const {
  if (today.revenue.cents == 0) {
    return 0;
  }
  return (today.gross_profit.cents * 100) / today.revenue.cents;
}

int RestaurantStats::days_in_operation() const {
  return static_cast<int>(history.size());
}
