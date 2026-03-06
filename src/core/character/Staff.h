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
  bool is_late() const { return is_late_; }
  bool is_sick() const { return is_sick_; }
  bool is_pregnant() const { return is_pregnant_; }
  bool stole_money() const { return stole_money_; }

  void set_late(bool late) { is_late_ = late; }
  void set_sick(bool sick) { is_sick_ = sick; }
  void set_pregnant(bool pregnant) { is_pregnant_ = pregnant; }
  void set_stole_money(bool stole) { stole_money_ = stole; }

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
};
