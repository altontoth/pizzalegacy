/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/character/Character.h"
#include "core/character/Training.h"
#include "core/finance/Money.h"

#include <optional>

class Staff {
public:
  Staff(core::character::JobRole job_role,
        core::character::Character character);

  // Use the default copy semantics.
  Staff(const Staff&) = default;
  Staff& operator=(const Staff&) = default;
  int time_until_back() const;
  bool is_away() const;
  void send_on_training(Training&);
  void payment(Money);
  Money payment() const;
  int pizzas_per_day() const;
  void pizzas_per_day(int ppd);

  // Adjust motivation with clamping [0, 100]
  void adjust_motivation(int change);

  // Stat adjustment methods for manager feedback
  void adjust_trustworthiness(int change);
  void adjust_honesty(int change);
  void adjust_health(int change);

  // Event flags (set when events occur)
  bool is_absent() const { return is_sick_ || is_pregnant_; }
  bool is_late() const { return is_late_; }
  bool is_sick() const { return is_sick_; }
  std::string_view absence_reason() const {
    if (is_sick_) {
      return "sick";
    }
    if (is_pregnant_) {
      return "pregnant";
    }
    return "";
  }
  bool is_pregnant() const { return is_pregnant_; }
  bool stole_money() const { return stole_money_; }
  bool wants_to_quit() const { return wants_to_quit_; }
  bool demands_raise() const { return demands_raise_; }
  bool recently_demanded_raise() const { return recently_demanded_raise_; }

  void late(bool late) { is_late_ = late; }
  void sick(bool sick) { is_sick_ = sick; }
  void pregnant(bool pregnant) { is_pregnant_ = pregnant; }
  void stole_money(bool stole) { stole_money_ = stole; }
  void wants_to_quit(bool quit) { wants_to_quit_ = quit; }
  void demands_raise(bool raise) { demands_raise_ = raise; }
  void recently_demanded_raise(bool v) { recently_demanded_raise_ = v; }

  // Duration-based absence (sick leave, pregnancy)
  void days_away(int days);
  void tick_days_away();
  int days_away_remaining() const { return days_away_remaining_; }

  // Calculate prep time in minutes (for chefs)
  // Derived from pizzas_per_day: time = operating_minutes / pizzas_per_day
  int calculate_prep_time_minutes() const;

  // Calculate service time in minutes (for waiters)
  // Derived from pizzas_per_day: time = operating_minutes / pizzas_per_day
  int calculate_service_time_minutes() const;

  const core::character::Character& character() const;
  core::character::JobRole job_role() const;

private:
  core::character::JobRole job_role_{core::character::JobRole::NONE};
  core::character::Character character_;
  Money payment_{0};
  std::optional<Training> current_training_;

  // Event flags
  bool is_late_{false};
  bool is_sick_{false};
  bool is_pregnant_{false};
  bool stole_money_{false};
  bool wants_to_quit_{false};
  bool demands_raise_{false};
  bool recently_demanded_raise_{false};

  // Duration-based absence counter (sick leave, pregnancy)
  int days_away_remaining_{0};
};
