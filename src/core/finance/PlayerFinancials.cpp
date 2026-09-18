/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/finance/PlayerFinancials.h"

#include <algorithm>
#include <numeric>

void PlayerFinancials::spend_cash(
    const GameDate& date, MonthlyCategory category, Money amount,
    std::optional<core::RestaurantId> restaurant_id) {
  post({
      date,
      {
          {account_for(category), amount.cents, restaurant_id, category},
          {Account::Cash, -amount.cents, restaurant_id},
      },
  });
}

void PlayerFinancials::transfer(const GameDate& date, Account from, Account to,
                                Money amount,
                                std::optional<MonthlyCategory> monthly) {
  transfer(date, from, to, amount, std::nullopt, monthly);
}

void PlayerFinancials::transfer(const GameDate& date, Account from, Account to,
                                Money amount,
                                std::optional<core::RestaurantId> restaurant_id,
                                std::optional<MonthlyCategory> monthly) {
  // The overview reads a positive amount as an expense and a negative one as
  // income, so the category goes on `from` for income and on `to` for
  // expenses.
  const bool receipt = monthly && is_receipt(*monthly);
  post({
      date,
      {
          {to, amount.cents, restaurant_id, receipt ? std::nullopt : monthly},
          {from, -amount.cents, restaurant_id,
           receipt ? monthly : std::nullopt},
      },
  });
}

void PlayerFinancials::pay_loan_installment(const GameDate& date,
                                            Money principal, Money interest) {
  post({
      date,
      {
          {Account::Loan, principal, std::nullopt, MonthlyCategory::Repayment},
          {Account::ProfitAndLoss, interest, std::nullopt,
           MonthlyCategory::Repayment},
          {Account::Cash, -(principal + interest)},
      },
  });
}

void PlayerFinancials::close_year(const GameDate& date) {
  const Money result = -balance(Account::ProfitAndLoss);
  if (result.cents == 0) {
    return;
  }
  constexpr int tax_share = 10;
  const Money tax = Money::from_dollars(result.whole() / tax_share);
  transfer(date, Account::Cash, Account::ProfitAndLoss, tax,
           balance_sheet_only);

  const Money profit_and_loss = balance(Account::ProfitAndLoss);
  post({
      date,
      {
          {Account::ProfitAndLoss, -profit_and_loss},
          {Account::Capital, profit_and_loss},
      },
  });
}

void PlayerFinancials::post(const JournalEntry& je) { ledger_.post(je); }

void PlayerFinancials::start_profit_day() {
  std::rotate(daily_profit_history_.begin(), daily_profit_history_.begin() + 1,
              daily_profit_history_.end());
  daily_profit_history_.back() = 0;
  if (daily_profit_days_ < daily_profit_days_max) {
    ++daily_profit_days_;
  }
}

void PlayerFinancials::add_to_profit_today(int64_t dollars) {
  daily_profit_history_.back() += dollars;
}

void PlayerFinancials::close_profit_day(core::CityId city) {
  weekly_city_profit_.at(city.value) =
      std::accumulate(daily_profit_history_.end() - days_per_week,
                      daily_profit_history_.end(), int64_t{0});
}

void PlayerFinancials::count_day_present(core::CityId city) {
  ++days_present_.at(city.value);
}

void PlayerFinancials::reset_days_present() { days_present_ = {}; }

void PlayerFinancials::arrive_in_city(core::CityId city, bool first_visit,
                                      int week_day) {
  if (first_visit) {
    daily_profit_history_ = {};
    daily_profit_days_ = 0;
    days_present_.at(city.value) = week_day;
  }
  else {
    daily_profit_history_.fill(weekly_city_profit_.at(city.value) /
                               days_per_week);
    daily_profit_days_ = daily_profit_days_max;
  }
}

void PlayerFinancials::set_weekly_city_profits(
    const std::array<int64_t, NUMBER_OF_CITIES>& profits,
    const std::array<int, NUMBER_OF_CITIES>& days_present) {
  weekly_city_profit_ = profits;
  days_present_ = days_present;
}

void PlayerFinancials::set_daily_profit_history(
    const std::array<int64_t, daily_profit_days_max>& history, int days) {
  daily_profit_history_ = history;
  daily_profit_days_ = std::clamp(days, 0, daily_profit_days_max);
}

Money PlayerFinancials::current_month(
    MonthlyCategory category, const GameDate& current_date,
    std::optional<core::RestaurantId> restaurant_id) const {
  const auto& figures = restaurant_id ? ledger_.restaurant_month(*restaurant_id)
                                      : ledger_.month();
  return figures.current_month(category, current_date);
}

Money PlayerFinancials::previous_month(
    MonthlyCategory category, const GameDate& current_date,
    std::optional<core::RestaurantId> restaurant_id) const {
  const auto& figures = restaurant_id ? ledger_.restaurant_month(*restaurant_id)
                                      : ledger_.month();
  return figures.previous_month(category, current_date);
}

void PlayerFinancials::start_liquidity(Money cash) {
  liquidity_ = {};
  liquidity_[0] = cash.whole();
  liquidity_weeks_ = 1;
}

void PlayerFinancials::record_liquidity(Money cash) {
  if (liquidity_weeks_ < liquidity_weeks_max) {
    ++liquidity_weeks_;
  }
  liquidity_[liquidity_weeks_ - 1] = cash.whole();
}

void PlayerFinancials::start_liquidity_year(bool fix_legacy_bugs) {
  // The original opens the new year with slot 49. The first year starts in
  // March and never reaches that slot, so its chart opens at zero. The fix
  // carries over the last slot written.
  const int carried =
      fix_legacy_bugs ? liquidity_weeks_ - 1 : liquidity_weeks_max - 1;
  const int64_t opening = liquidity_[carried];
  liquidity_ = {};
  liquidity_[0] = opening;
  liquidity_weeks_ = 1;
}

void PlayerFinancials::set_liquidity(
    const std::array<int64_t, liquidity_weeks_max>& weeks, int count) {
  liquidity_ = weeks;
  liquidity_weeks_ = std::clamp(count, 1, liquidity_weeks_max);
}
