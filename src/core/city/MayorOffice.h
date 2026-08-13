/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include "core/finance/Money.h"
#include "core/time/GameDate.h"
#include "core/util/Constants.h"
#include "engine/Random.h"

class BankAccountRegistry;

// Award types produced by MayorOffice scheduling. GameState maps these to
// MayorConversationType for the conversation/UI layer.
enum class MayorAwardType {
  None,
  MostBeautifulRestaurant,
  PizzaOfTheYear,
  BestInvestorInTown,
  BestImitator,
  GoldenBowTie,
  TransAtlanticTrade,
};

struct MayorAwardEvent {
  MayorAwardType type{MayorAwardType::None};
  // nullopt = announcement (no winner determined yet).
  // Some(id) = ceremony; id is the winning player index, or -1 if nobody
  // qualified (used by awards that have a "nobody won" dialog).
  std::optional<int> winner_player_id{std::nullopt};
  bool is_ceremony{false};
};

class MayorOffice {
public:
  void register_accounts(BankAccountRegistry& registry);
  void start_of_day(const GameDate& date, BankAccountRegistry& registry);

  int public_bank_account() const { return public_bank_account_; }
  void public_bank_account(int account) { public_bank_account_ = account; }
  int private_bank_account() const { return private_bank_account_; }
  void private_bank_account(int account) { private_bank_account_ = account; }

  const std::optional<GameDate>& last_rotation_date() const {
    return last_rotation_date_;
  }
  void last_rotation_date(const std::optional<GameDate>& date) {
    last_rotation_date_ = date;
  }

  // Mayor favor score for a player (0-200). Increased by donations to the
  // private account; decays 1/day. Used as a % chance to pass inspections.
  int favor_score(int player_id) const { return favor_score_[player_id]; }
  void add_favor(int player_id, Money amount);
  void favor(int player_id, int value) {
    favor_score_[player_id] = std::clamp(value, 0, 200);
  }

  // Per-player investment and trade activity totals (used in award scoring).
  void add_investment(int player_id, int32_t amount);
  void add_trade(int player_id, int32_t amount);
  int32_t investment_activity(int player_id) const {
    return investment_activity_[player_id];
  }
  int32_t trade_activity(int player_id) const {
    return trade_activity_[player_id];
  }

  // Award participation fee payment (one entry per player).
  void fee_paid(int player_id);
  bool fee_paid(int player_id) const;
  void clear_fee_paid() { fee_paid_.fill(false); }

  // Appointment system for player-initiated phone calls.
  // appointment_day() is the day-of-month when the meeting can happen.
  int appointment_day() const { return appointment_day_; }
  void advance_appointment(const GameDate& date);

  // Per-player pizza competition win count for this city.
  // Must exceed 4 wins to qualify for the Best Imitator award.
  int pizza_competition_wins(int player_id) const {
    return pizza_competition_wins_[player_id];
  }
  void add_pizza_competition_win(int player_id) {
    pizza_competition_wins_[player_id]++;
  }

  // Save/load accessors for fields with no other public setter.
  void pizza_competition_wins(int player_id, int value) {
    pizza_competition_wins_[player_id] = value;
  }
  void investment_activity(int player_id, int32_t value) {
    investment_activity_[player_id] = value;
  }
  void trade_activity(int player_id, int32_t value) {
    trade_activity_[player_id] = value;
  }
  void appointment_day(int day) { appointment_day_ = day; }
  MayorAwardType current_quarterly_award() const {
    return current_quarterly_award_;
  }
  void current_quarterly_award(MayorAwardType t) {
    current_quarterly_award_ = t;
  }
  int award_announcement_day() const { return announcement_day_; }
  void award_announcement_day(int d) { announcement_day_ = d; }
  int award_ceremony_day() const { return ceremony_day_; }
  void award_ceremony_day(int d) { ceremony_day_ = d; }

  // Called from GameState::run_city_start_of_day().
  // Returns any award events that should fire today.
  // Also decays favor scores and advances the appointment system.
  std::vector<MayorAwardEvent>
  process_daily_scheduling(const GameDate& date, int player_count,
                           BankAccountRegistry& registry);

private:
  void schedule_quarterly_award(const GameDate& date,
                                BankAccountRegistry& registry);
  // Compute the winner for the currently scheduled quarterly award.
  // Returns -1 if nobody is eligible.
  std::optional<int> compute_award_winner(MayorAwardType type,
                                          int player_count) const;

  int public_bank_account_{engine::random::rand_max_n(999'999)};
  int private_bank_account_{engine::random::rand_max_n(99'999'999)};

  // Mayor favor per player: incremented by donations / 1000, cap 200.
  std::array<int, NUMBER_OF_PLAYERS> favor_score_{};
  std::array<int32_t, NUMBER_OF_PLAYERS> investment_activity_{};
  std::array<int32_t, NUMBER_OF_PLAYERS> trade_activity_{};

  // Per-player flag: true when the player has paid the $2000 award fee.
  std::array<bool, NUMBER_OF_PLAYERS> fee_paid_{};

  // Per-player pizza competition win count for this city.
  std::array<int, NUMBER_OF_PLAYERS> pizza_competition_wins_{};

  // Quarterly award scheduling (values are day-of-month within next month).
  MayorAwardType current_quarterly_award_{MayorAwardType::None};
  int announcement_day_{0}; // 0 = not scheduled
  int ceremony_day_{0};     // 0 = not scheduled

  // Day-of-month when the player can meet the mayor for a donation.
  int appointment_day_{0};

  // Month-start date of the last private account rotation. nullopt = never
  // rotated. Rotation happens lazily on first city visit each month, even if
  // the city was unvisited on the 1st.
  std::optional<GameDate> last_rotation_date_;
};
