/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/MayorOffice.h"
#include "core/finance/BankAccountRegistry.h"
#include "engine/Random.h"

void MayorOffice::register_accounts(BankAccountRegistry& registry) {
  registry.register_account(public_bank_account_);
  registry.register_account(private_bank_account_);
}

void MayorOffice::end_of_day(const GameDate& date,
                             BankAccountRegistry& registry) {
  // The long/private account is regenerated on the 1st of every month.
  // The short/public account is only rotated when a quarterly award is
  // scheduled (see schedule_quarterly_award).
  if (date.day() == 1) {
    registry.close_account(private_bank_account_);
    private_bank_account_ = engine::random::rand_max_n(99'999'999);
    registry.register_account(private_bank_account_);
  }
}

void MayorOffice::add_favor(int player_id, Money amount) {
  constexpr int max_favor = 200;
  int gain = static_cast<int>(amount.cents / 100'000); // amount/1000 in dollars
  favor_score_[player_id] = std::min(max_favor, favor_score_[player_id] + gain);
}

void MayorOffice::add_investment(int player_id, int32_t amount) {
  investment_activity_[player_id] += amount;
}

void MayorOffice::add_trade(int player_id, int32_t amount) {
  trade_activity_[player_id] += amount;
}

void MayorOffice::set_fee_paid(int player_id) { fee_paid_[player_id] = true; }

bool MayorOffice::fee_paid(int player_id) const { return fee_paid_[player_id]; }

void MayorOffice::advance_appointment(const GameDate& date) {
  // 10% chance each day to schedule a new appointment 1-10 days out
  if (engine::random::rand_percentage_chance(10)) {
    appointment_day_ =
        static_cast<int>(date.day()) + engine::random::rand_max_n(10) + 1;
    if (appointment_day_ > 30) {
      appointment_day_ -= 30;
    }
  }
}

std::vector<MayorAwardEvent>
MayorOffice::process_daily_scheduling(const GameDate& date, int player_count,
                                      BankAccountRegistry& registry) {
  std::vector<MayorAwardEvent> events;

  // Decay favor scores by 1 per day for all active players
  for (int i = 0; i < player_count; i++) {
    if (favor_score_[i] > 0) {
      favor_score_[i]--;
    }
  }

  // Advance appointment system
  advance_appointment(date);

  auto day = static_cast<int>(date.day());
  auto month = static_cast<int>(date.month());

  // June 30: Best Imitator of the Year (always a ceremony)
  if (month == 6 && day == 30) {
    std::optional<int> winner =
        compute_award_winner(MayorAwardType::BestImitator, player_count);
    events.push_back({.type = MayorAwardType::BestImitator,
                      .winner_player_id = winner,
                      .is_ceremony = true});
    return events;
  }

  // December 31: Pizza of the Year (always a ceremony)
  // TODO: Pizza of the Year involves player-interactive pizza design.
  // The winner is not yet determined here; GameState must resolve it.
  if (month == 12 && day == 31) {
    events.push_back({.type = MayorAwardType::PizzaOfTheYear,
                      .winner_player_id = std::nullopt,
                      .is_ceremony = true});
    return events;
  }

  // Last day of every 3rd month: schedule a quarterly award for next month
  if (date.is_end_of_month() && month % 3 == 0) {
    schedule_quarterly_award(date, registry);
  }

  // Quarterly award announcement: no winner yet, fee text shown to players
  if (current_quarterly_award_ != MayorAwardType::None &&
      announcement_day_ > 0 && day == announcement_day_) {
    events.push_back(
        {.type = current_quarterly_award_, .winner_player_id = std::nullopt});
    announcement_day_ = 0; // consumed
  }

  // Quarterly award ceremony: compute winner and deliver result
  if (current_quarterly_award_ != MayorAwardType::None && ceremony_day_ > 0 &&
      day == ceremony_day_) {
    std::optional<int> winner =
        compute_award_winner(current_quarterly_award_, player_count);
    events.push_back({.type = current_quarterly_award_,
                      .winner_player_id = winner,
                      .is_ceremony = true});
    current_quarterly_award_ = MayorAwardType::None;
    ceremony_day_ = 0;
    clear_fee_paid();
  }

  return events;
}

void MayorOffice::schedule_quarterly_award(const GameDate& /*date*/,
                                           BankAccountRegistry& registry) {
  // Original probabilities (with override chain):
  // 30% Best Investor, 20% Golden Bow Tie, 20% Trans-Atlantic Trade, ~45% none
  MayorAwardType award = MayorAwardType::None;

  if (engine::random::rand_percentage_chance(30)) {
    award = MayorAwardType::BestInvestorInTown;
  }
  if (engine::random::rand_percentage_chance(20)) {
    award = MayorAwardType::GoldenBowTie;
  }
  if (engine::random::rand_percentage_chance(20)) {
    award = MayorAwardType::TransAtlanticTrade;
  }

  current_quarterly_award_ = award;

  if (award != MayorAwardType::None) {
    announcement_day_ = engine::random::rand_max_n(5) + 1;
    ceremony_day_ = engine::random::rand_max_n(10) + 10;
    registry.close_account(public_bank_account_);
    public_bank_account_ = engine::random::rand_max_n(999'999);
    registry.register_account(public_bank_account_);
  }
}

std::optional<int> MayorOffice::compute_award_winner(MayorAwardType type,
                                                     int player_count) const {
  // MostBeautifulRestaurant and GoldenBowTie require iterating restaurant
  // furniture/service data not accessible here. Computed by GameState.
  switch (type) {
  case MayorAwardType::BestInvestorInTown: {
    int best_score = 0;
    std::optional<int> winner;
    for (int i = 0; i < player_count; i++) {
      if (!fee_paid(i)) {
        continue;
      }
      int score = (favor_score_[i] * 100) + investment_activity_[i];
      if (score > best_score) {
        best_score = score;
        winner = i;
      }
    }
    return winner;
  }
  case MayorAwardType::TransAtlanticTrade: {
    int32_t best_score = 0;
    std::optional<int> winner;
    for (int i = 0; i < player_count; i++) {
      int32_t score =
          (static_cast<int32_t>(favor_score_[i]) * 100) + trade_activity_[i];
      if (score > best_score) {
        best_score = score;
        winner = i;
      }
    }
    return winner;
  }
  case MayorAwardType::BestImitator: {
    // Must exceed 4 pizza competition wins in this city to be eligible.
    constexpr int min_wins = 4;
    int best_wins = min_wins;
    std::optional<int> winner;
    for (int i = 0; i < player_count; i++) {
      if (pizza_competition_wins_[i] > best_wins) {
        best_wins = pizza_competition_wins_[i];
        winner = i;
      }
    }
    return winner;
  }
  default:
    // MostBeautifulRestaurant, GoldenBowTie, PizzaOfTheYear:
    // winner determined by GameState which has access to restaurant data.
    return std::nullopt;
  }
}
