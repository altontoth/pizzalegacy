/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include "core/common/Ids.h"
#include "core/finance/Ledger.h"
#include "core/util/Constants.h"

class PlayerFinancials {
  friend std::ostream& operator<<(std::ostream& os,
                                  const PlayerFinancials& pf) {
    os << "Player financials:\n";
    os << pf.ledger_;
    return os;
  }

public:
  void
  spend_cash(const GameDate& date, MonthlyCategory category, Money amount,
             std::optional<core::RestaurantId> restaurant_id = std::nullopt);

  void transfer(const GameDate& date, Account from, Account to, Money amount,
                std::optional<MonthlyCategory> monthly);
  void transfer(const GameDate& date, Account from, Account to, Money amount,
                std::optional<core::RestaurantId> restaurant_id,
                std::optional<MonthlyCategory> monthly);

  void pay_loan_installment(const GameDate& date, Money principal,
                            Money interest);

  // Takes 10% tax on the year's profit, or refunds 10% of a loss, and moves
  // the rest into capital.
  void close_year(const GameDate& date);

  Money balance(Account account) const { return ledger_.balance(account); }

  Money current_month(
      MonthlyCategory category, const GameDate& current_date,
      std::optional<core::RestaurantId> restaurant_id = std::nullopt) const;
  Money previous_month(
      MonthlyCategory category, const GameDate& current_date,
      std::optional<core::RestaurantId> restaurant_id = std::nullopt) const;

  const Ledger& ledger() const noexcept { return ledger_; }
  Ledger& ledger() noexcept { return ledger_; }

  // The daily profits chart for the city the player is in, in whole dollars,
  // most recent day last.
  static constexpr int daily_profit_days_max = 30;
  static constexpr int days_per_week = 7;
  static constexpr int days_per_rent_month = 30;
  void start_profit_day();
  void add_to_profit_today(int64_t dollars);
  // Called once a day. The city's weekly profit becomes the sum of the last
  // seven days.
  void close_profit_day(core::CityId city);
  void set_daily_profit_history(
      const std::array<int64_t, daily_profit_days_max>& history, int days);
  const std::array<int64_t, daily_profit_days_max>&
  daily_profit_history() const {
    return daily_profit_history_;
  }
  int daily_profit_days() const { return daily_profit_days_; }

  // A city is paid its weekly profit for the days of the week the player was
  // not there.
  void count_day_present(core::CityId city);
  void reset_days_present();
  void arrive_in_city(core::CityId city, bool first_visit, int week_day);
  int64_t weekly_city_profit(core::CityId city) const {
    return weekly_city_profit_.at(city.value);
  }
  int days_present(core::CityId city) const {
    return days_present_.at(city.value);
  }
  void set_weekly_city_profits(
      const std::array<int64_t, NUMBER_OF_CITIES>& profits,
      const std::array<int, NUMBER_OF_CITIES>& days_present);
  const std::array<int64_t, NUMBER_OF_CITIES>& weekly_city_profits() const {
    return weekly_city_profit_;
  }
  const std::array<int, NUMBER_OF_CITIES>& days_present() const {
    return days_present_;
  }

  // Cash at the end of each week of the year for the liquidity chart. Slot 0
  // holds the cash the year started with, and the last slot takes every week
  // once the others are full.
  static constexpr int liquidity_weeks_max = 50;
  void start_liquidity(Money cash);
  void record_liquidity(Money cash);
  void start_liquidity_year(bool fix_legacy_bugs);
  void set_liquidity(const std::array<int64_t, liquidity_weeks_max>& weeks,
                     int count);
  const std::array<int64_t, liquidity_weeks_max>& liquidity() const {
    return liquidity_;
  }
  int liquidity_weeks() const { return liquidity_weeks_; }

private:
  void post(const JournalEntry& je);

  Ledger ledger_;
  std::array<int64_t, daily_profit_days_max> daily_profit_history_{};
  int daily_profit_days_ = 0;
  std::array<int64_t, NUMBER_OF_CITIES> weekly_city_profit_{};
  std::array<int, NUMBER_OF_CITIES> days_present_{};
  std::array<int64_t, liquidity_weeks_max> liquidity_{};
  int liquidity_weeks_ = 0;
};
