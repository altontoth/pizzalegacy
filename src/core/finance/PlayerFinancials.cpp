/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/finance/PlayerFinancials.h"

#include <algorithm>

// spend_cash: Always operational
void PlayerFinancials::spend_cash(
    const GameDate& date, Account to, Money amount,
    std::optional<core::RestaurantId> restaurant_id) {
  post({
      date,
      {
          {to, amount.cents, restaurant_id, AccountingScope::Operational},
          {Account::Cash, -amount.cents, restaurant_id,
           AccountingScope::Operational},
      },
  });
}

// transfer: Explicit scope for capital/operational movements
void PlayerFinancials::transfer(const GameDate& date, Account from, Account to,
                                Money amount, AccountingScope scope) {
  post({
      date,
      {
          {to, amount.cents, std::nullopt, scope},
          {from, -amount.cents, std::nullopt, scope},
      },
  });
}

void PlayerFinancials::transfer(const GameDate& date, Account from, Account to,
                                Money amount, core::RestaurantId restaurant_id,
                                AccountingScope scope) {
  post({
      date,
      {
          {to, amount.cents, restaurant_id, scope},
          {from, -amount.cents, restaurant_id, scope},
      },
  });
}
void PlayerFinancials::close_year(const GameDate& date) {
  int64_t profit = 0; // ledger_.balance(Account::ProfitAndLoss);
  if (profit == 0) {
    return;
  }

  ledger_.post(
      {date, {{Account::Capital, profit}, {Account::ProfitAndLoss, -profit}}});
}
// Static methods for account categorization
std::vector<Account> PlayerFinancials::receipt_accounts() {
  return {
      Account::Sales,
      Account::CoinMachines,
      Account::Missions,
  };
}

std::vector<Account> PlayerFinancials::expenditure_accounts() {
  return {
      Account::Staff,     Account::Rents,     Account::Stock,
      Account::Machines,  Account::Furniture, Account::Protection,
      Account::Repayment, Account::Others,
  };
}
// Methods for calculating category totals
Money PlayerFinancials::total_receipts(
    std::optional<uint32_t> restaurant) const {
  Money total = 0;
  std::optional<core::RestaurantId> rid =
      restaurant.has_value()
          ? std::optional<core::RestaurantId>{core::RestaurantId{
                static_cast<int32_t>(*restaurant)}}
          : std::nullopt;
  for (Account account : receipt_accounts()) {
    total += ledger_.balance(account, rid);
  }
  return total;
}

Money PlayerFinancials::total_expenditures(
    std::optional<uint32_t> restaurant) const {
  Money total = 0;
  std::optional<core::RestaurantId> rid =
      restaurant.has_value()
          ? std::optional<core::RestaurantId>{core::RestaurantId{
                static_cast<int32_t>(*restaurant)}}
          : std::nullopt;
  for (Account account : expenditure_accounts()) {
    total += ledger_.balance(account, rid);
  }
  return total;
}

// Overloads with date range filtering
Money PlayerFinancials::total_receipts(
    GameDate from, GameDate to, std::optional<uint32_t> restaurant) const {
  Money total = 0;
  std::optional<core::RestaurantId> rid =
      restaurant.has_value()
          ? std::optional<core::RestaurantId>{core::RestaurantId{
                static_cast<int32_t>(*restaurant)}}
          : std::nullopt;
  for (Account account : receipt_accounts()) {
    total += ledger_.balance_between(account, from, to, rid);
  }
  return total;
}

Money PlayerFinancials::total_expenditures(
    GameDate from, GameDate to, std::optional<uint32_t> restaurant) const {
  Money total = 0;
  std::optional<core::RestaurantId> rid =
      restaurant.has_value()
          ? std::optional<core::RestaurantId>{core::RestaurantId{
                static_cast<int32_t>(*restaurant)}}
          : std::nullopt;
  for (Account account : expenditure_accounts()) {
    total += ledger_.balance_between(account, from, to, rid);
  }
  return total;
}

void PlayerFinancials::post(const JournalEntry& je) {
  ledger_.post(je);
  cached_cash_ = 0; // ledger_.balance(Account::Cash);
}

void PlayerFinancials::record_daily_profit(Money profit) {
  std::rotate(daily_profit_history_.begin(), daily_profit_history_.begin() + 1,
              daily_profit_history_.end());
  daily_profit_history_.back() = profit.whole();
  if (daily_profit_days_ < daily_profit_days_max) {
    ++daily_profit_days_;
  }
}

void PlayerFinancials::set_daily_profit_history(
    const std::array<int64_t, daily_profit_days_max>& history, int days) {
  daily_profit_history_ = history;
  daily_profit_days_ = std::clamp(days, 0, daily_profit_days_max);
}

Money PlayerFinancials::current_month_balance(
    Account account, const GameDate& current_date,
    std::optional<uint32_t> restaurant) const {
  std::optional<core::RestaurantId> rid = std::nullopt;
  if (restaurant.has_value()) {
    rid = core::RestaurantId{static_cast<int32_t>(*restaurant)};
  }
  return ledger_.balance_between(account, current_date.month_start(),
                                 current_date.month_end(), rid);
}

Money PlayerFinancials::previous_month_balance(
    Account account, const GameDate& current_date,
    std::optional<uint32_t> restaurant) const {
  std::optional<core::RestaurantId> rid = std::nullopt;
  if (restaurant.has_value()) {
    rid = core::RestaurantId{static_cast<int32_t>(*restaurant)};
  }
  return ledger_.balance_between(account, current_date.previous_month_start(),
                                 current_date.previous_month_end(), rid);
}

Money PlayerFinancials::balance(Account account) const {
  return ledger_.balance(account);
}

// Query methods with scope filtering
Money PlayerFinancials::current_month_operational(
    Account account, const GameDate& current_date) const {
  return ledger_.balance_between(account, current_date.month_start(),
                                 current_date.month_end(), std::nullopt,
                                 AccountingScope::Operational);
}

Money PlayerFinancials::current_month_operational(
    Account account, const GameDate& current_date,
    core::RestaurantId restaurant_id) const {
  return ledger_.balance_between(account, current_date.month_start(),
                                 current_date.month_end(), restaurant_id,
                                 AccountingScope::Operational);
}

Money PlayerFinancials::previous_month_operational(
    Account account, const GameDate& current_date) const {
  return ledger_.balance_between(account, current_date.previous_month_start(),
                                 current_date.previous_month_end(),
                                 std::nullopt, AccountingScope::Operational);
}

Money PlayerFinancials::previous_month_operational(
    Account account, const GameDate& current_date,
    core::RestaurantId restaurant_id) const {
  return ledger_.balance_between(account, current_date.previous_month_start(),
                                 current_date.previous_month_end(),
                                 restaurant_id, AccountingScope::Operational);
}

Money PlayerFinancials::balance_between(
    Account acct, GameDate from, GameDate to,
    std::optional<uint32_t> restaurant) const {
  std::optional<core::RestaurantId> rid =
      restaurant.has_value()
          ? std::optional<core::RestaurantId>{core::RestaurantId{
                static_cast<int32_t>(*restaurant)}}
          : std::nullopt;
  return ledger_.balance_between(acct, from, to, rid);
}
