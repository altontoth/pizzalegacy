/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <deque>
#include <iomanip>
#include <map>
#include <optional>
#include <ostream>
#include <string_view>
#include <vector>
#include "core/common/Ids.h"
#include "core/finance/Money.h"
#include "core/time/GameDate.h"

// The lines of the balance sheet.
enum class Account {
  Property,
  Machines,
  Furniture,
  Weapons,
  Stock,
  Cash,
  Capital,
  Loan,
  // What is owed to suppliers for contract deliveries.
  DeliveryDebts,
  // The original's Balance page labels it; nothing in the game posts to it.
  Debts,
  // This year's profit or loss so far.
  ProfitAndLoss,
};

inline constexpr std::size_t account_count =
    static_cast<std::size_t>(Account::ProfitAndLoss) + 1;

// The receipt and expenditure lines of the monthly overview.
enum class MonthlyCategory {
  Sales,
  CoinMachines,
  Missions,
  Staff,
  Rents,
  Stock,
  Machines,
  Furniture,
  Protection,
  Repayment,
  Others,
};

inline constexpr std::size_t category_count =
    static_cast<std::size_t>(MonthlyCategory::Others) + 1;

inline constexpr std::array receipt_categories = {
    MonthlyCategory::Sales,
    MonthlyCategory::CoinMachines,
    MonthlyCategory::Missions,
};

inline constexpr std::array expenditure_categories = {
    MonthlyCategory::Staff,     MonthlyCategory::Rents,
    MonthlyCategory::Stock,     MonthlyCategory::Machines,
    MonthlyCategory::Furniture, MonthlyCategory::Protection,
    MonthlyCategory::Repayment, MonthlyCategory::Others,
};

inline bool is_receipt(MonthlyCategory category) {
  return std::ranges::find(receipt_categories, category) !=
         receipt_categories.end();
}

// The monthly category of a transfer not shown in the overview.
inline constexpr std::nullopt_t balance_sheet_only = std::nullopt;

inline std::string_view to_string(Account account) {
  switch (account) {
  case Account::Property:
    return "property";
  case Account::Machines:
    return "machines";
  case Account::Furniture:
    return "furniture";
  case Account::Weapons:
    return "weapons";
  case Account::Stock:
    return "stock";
  case Account::Cash:
    return "cash";
  case Account::Capital:
    return "capital";
  case Account::Loan:
    return "loan";
  case Account::DeliveryDebts:
    return "delivery debts";
  case Account::Debts:
    return "debts";
  case Account::ProfitAndLoss:
    return "profit and loss";
  }
  return "unknown";
}

inline std::string_view to_string(MonthlyCategory category) {
  switch (category) {
  case MonthlyCategory::Sales:
    return "sales";
  case MonthlyCategory::CoinMachines:
    return "coin machines";
  case MonthlyCategory::Missions:
    return "missions";
  case MonthlyCategory::Staff:
    return "staff";
  case MonthlyCategory::Rents:
    return "rents";
  case MonthlyCategory::Stock:
    return "stock";
  case MonthlyCategory::Machines:
    return "machines";
  case MonthlyCategory::Furniture:
    return "furniture";
  case MonthlyCategory::Protection:
    return "protection";
  case MonthlyCategory::Repayment:
    return "repayment";
  case MonthlyCategory::Others:
    return "others";
  }
  return "unknown";
}

// Where money spent under a category goes on the balance sheet.
inline Account account_for(MonthlyCategory category) {
  switch (category) {
  case MonthlyCategory::Stock:
    return Account::Stock;
  case MonthlyCategory::Machines:
    return Account::Machines;
  case MonthlyCategory::Furniture:
    return Account::Furniture;
  default:
    return Account::ProfitAndLoss;
  }
}

struct Posting {
  Account account;
  Money amount; // plus = debit, minus = credit
  std::optional<core::RestaurantId> restaurant_id;
  // The overview category this line counts toward, if any.
  std::optional<MonthlyCategory> monthly;

  Posting(Account acc, Money amt,
          std::optional<core::RestaurantId> rest_id = std::nullopt,
          std::optional<MonthlyCategory> monthly_category = std::nullopt)
      : account(acc), amount(amt), restaurant_id(rest_id),
        monthly(monthly_category) {}
};

struct JournalEntry {
  GameDate date;
  std::vector<Posting> lines;
};

// This month's and last month's overview numbers, expenses positive and
// receipts negative. The first booking in a new month moves this month's
// numbers to last month.
class MonthlyFigures {
public:
  using Figures = std::array<Money, category_count>;

  void book(const GameDate& date, MonthlyCategory category, Money amount);
  Money current_month(MonthlyCategory category, const GameDate& date) const;
  Money previous_month(MonthlyCategory category, const GameDate& date) const;

  static int month_number(const GameDate& date) {
    return (date.year() * 12) + date.month() - 1;
  }
  int month() const { return month_; }
  const Figures& this_month() const { return this_month_; }
  const Figures& last_month() const { return last_month_; }
  void set(int month, const Figures& this_month, const Figures& last_month);

private:
  int month_ = 0;
  Figures this_month_{};
  Figures last_month_{};
};

// The running balance of every account and the monthly overview numbers of
// the player and of each restaurant. The most recent postings are kept for
// dump ledger.
class Ledger {
  friend std::ostream& operator<<(std::ostream& os, const Ledger& ledger) {
    for (const auto& entry : ledger.recent_) {
      os << entry.date << ":\n";
      for (const auto& line : entry.lines) {
        os << "  " << std::setw(16) << to_string(line.account) << "  "
           << std::setw(10) << line.amount;
        if (line.monthly.has_value()) {
          os << "  [" << to_string(*line.monthly) << "]";
        }
        if (line.restaurant_id.has_value()) {
          os << "  (restaurant #" << line.restaurant_id.value() << ")";
        }
        os << "\n";
      }
    }
    return os;
  }

public:
  static constexpr std::size_t recent_max = 500;

  void post(const JournalEntry& je);
  Money balance(Account account) const {
    return balances_.at(static_cast<std::size_t>(account));
  }
  const MonthlyFigures& month() const { return month_; }
  const MonthlyFigures& restaurant_month(core::RestaurantId id) const;
  const std::map<core::RestaurantId, MonthlyFigures>&
  restaurant_months() const {
    return restaurant_months_;
  }

  void set_balance(Account account, Money amount) {
    balances_.at(static_cast<std::size_t>(account)) = amount;
  }
  void set_month(const MonthlyFigures& figures) { month_ = figures; }
  void set_restaurant_month(core::RestaurantId id,
                            const MonthlyFigures& figures) {
    restaurant_months_[id] = figures;
  }

private:
  std::array<Money, account_count> balances_{};
  MonthlyFigures month_;
  std::map<core::RestaurantId, MonthlyFigures> restaurant_months_;
  std::deque<JournalEntry> recent_;
};
