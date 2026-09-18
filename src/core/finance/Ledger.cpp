/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/finance/Ledger.h"

#include <stdexcept>

void MonthlyFigures::book(const GameDate& date, MonthlyCategory category,
                          Money amount) {
  const int month = month_number(date);
  const auto index = static_cast<std::size_t>(category);
  if (month > month_) {
    last_month_ = month == month_ + 1 ? this_month_ : Figures{};
    this_month_ = {};
    month_ = month;
  }
  if (month == month_) {
    this_month_.at(index) += amount;
  }
  else if (month == month_ - 1) {
    last_month_.at(index) += amount;
  }
}

Money MonthlyFigures::current_month(MonthlyCategory category,
                                    const GameDate& date) const {
  if (month_number(date) != month_) {
    return Money{0};
  }
  return this_month_.at(static_cast<std::size_t>(category));
}

Money MonthlyFigures::previous_month(MonthlyCategory category,
                                     const GameDate& date) const {
  const int month = month_number(date);
  const auto index = static_cast<std::size_t>(category);
  if (month == month_) {
    return last_month_.at(index);
  }
  if (month == month_ + 1) {
    return this_month_.at(index);
  }
  return Money{0};
}

void MonthlyFigures::set(int month, const Figures& this_month,
                         const Figures& last_month) {
  month_ = month;
  this_month_ = this_month;
  last_month_ = last_month;
}

void Ledger::post(const JournalEntry& je) {
  Money sum = 0_usd;
  for (const auto& p : je.lines) {
    sum += p.amount;
  }
  if (sum != 0_usd) {
    throw std::logic_error("Unbalanced JE");
  }
  for (const auto& p : je.lines) {
    balances_.at(static_cast<std::size_t>(p.account)) += p.amount;
    if (p.monthly) {
      month_.book(je.date, *p.monthly, p.amount);
      if (p.restaurant_id) {
        restaurant_months_[*p.restaurant_id].book(je.date, *p.monthly,
                                                  p.amount);
      }
    }
  }
  recent_.push_back(je);
  if (recent_.size() > recent_max) {
    recent_.pop_front();
  }
}

const MonthlyFigures& Ledger::restaurant_month(core::RestaurantId id) const {
  static const MonthlyFigures none;
  auto it = restaurant_months_.find(id);
  return it == restaurant_months_.end() ? none : it->second;
}
