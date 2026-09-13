/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <unordered_map>
#include "core/common/Ids.h"
#include "core/finance/Ledger.h"
#include "core/finance/Loan.h"

class PlayerFinancials {
  friend std::ostream& operator<<(std::ostream& os,
                                  const PlayerFinancials& pf) {
    os << "Player financials:\n";
    os << pf.ledger_;
    return os;
  }

public:
  // spend_cash: Always operational, never capital
  // For capital movements, use transfer() with explicit scope
  void
  spend_cash(const GameDate& date, Account to, Money amount,
             std::optional<core::RestaurantId> restaurant_id = std::nullopt);

  // transfer: For explicit accounting movements (including capital)
  // Player-level (no restaurant)
  void transfer(const GameDate& date, Account from, Account to, Money amount,
                AccountingScope scope);

  // Restaurant-specific
  void transfer(const GameDate& date, Account from, Account to, Money amount,
                core::RestaurantId restaurant_id, AccountingScope scope);

  void pay_loan_installment(const GameDate& date, Money principal,
                            Money interest);

  void close_year(const GameDate& date);

  // Static methods for account categorization
  static std::vector<Account> receipt_accounts();
  static std::vector<Account> expenditure_accounts();

  // Methods for calculating category totals. A player total counts every
  // posting, including the many that name no restaurant, so it adds up to
  // more than the per restaurant figures.
  Money total_receipts(std::optional<uint32_t> restaurant = std::nullopt) const;
  Money
  total_expenditures(std::optional<uint32_t> restaurant = std::nullopt) const;

  Money total_receipts(GameDate from, GameDate to,
                       std::optional<uint32_t> restaurant = std::nullopt) const;
  Money
  total_expenditures(GameDate from, GameDate to,
                     std::optional<uint32_t> restaurant = std::nullopt) const;
  Money
  balance_between(Account acct, GameDate from, GameDate to,
                  std::optional<uint32_t> restaurant = std::nullopt) const;
  Money current_month_balance(
      Account account, const GameDate& current_date,
      std::optional<uint32_t> restaurant = std::nullopt) const;
  Money previous_month_balance(
      Account account, const GameDate& current_date,
      std::optional<uint32_t> restaurant = std::nullopt) const;
  Money balance(Account account) const;

  // New query methods with scope filtering
  Money current_month_operational(Account account,
                                  const GameDate& current_date) const;
  Money current_month_operational(Account account, const GameDate& current_date,
                                  core::RestaurantId restaurant_id) const;
  Money previous_month_operational(Account account,
                                   const GameDate& current_date) const;
  Money previous_month_operational(Account account,
                                   const GameDate& current_date,
                                   core::RestaurantId restaurant_id) const;

  // For save/load only: direct ledger access
  const Ledger& ledger() const noexcept { return ledger_; }
  void restore_ledger(Ledger l) { ledger_ = std::move(l); }

  // Rolling daily-profit series for the daily-profits screen (index
  // daily_profit_days_max-1 = most recent day).
  // In normal play record_daily_profit appends each day's ledger and on load
  // it's restored directly.
  static constexpr int daily_profit_days_max = 30;
  void record_daily_profit(Money profit);
  void set_daily_profit_history(
      const std::array<int64_t, daily_profit_days_max>& history, int days);
  const std::array<int64_t, daily_profit_days_max>&
  daily_profit_history() const {
    return daily_profit_history_;
  }
  int daily_profit_days() const { return daily_profit_days_; }

private:
  void post(const JournalEntry& je);

  Ledger ledger_;
  std::vector<Loan> loans_;
  std::unordered_map<uint8_t, int> restaurant_data_;
  int64_t cached_cash_ = 0;
  std::array<int64_t, daily_profit_days_max> daily_profit_history_{};
  int daily_profit_days_ = 0;
};
