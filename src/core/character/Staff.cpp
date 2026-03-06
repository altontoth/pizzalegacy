/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/character/Staff.h"

#include <algorithm>

Staff::Staff(core::character::JobRole job_role,
             core::character::Character character)
    : job_role_(job_role), character_(character) {}

int Staff::time_until_back() const { return 0; }
bool Staff::is_away() const { return false; }
void Staff::send_on_training(Training& training) {
  current_training_ = training;
}
void Staff::payment(Money p) { payment_ = p; }
Money Staff::payment() const { return payment_; }

void Staff::adjust_motivation(int change) {
  int new_motivation = character_.motivation() + change;
  new_motivation = std::clamp(new_motivation, 0, 100);
  character_.motivation(static_cast<uint8_t>(new_motivation));
}

void Staff::adjust_trustworthiness(int change) {
  int new_trust = character_.trustworthiness() + change;
  new_trust = std::clamp(new_trust, 0, 100);
  character_.trustworthiness(static_cast<uint8_t>(new_trust));
}

void Staff::adjust_honesty(int change) {
  int new_honesty = character_.honesty() + change;
  new_honesty = std::clamp(new_honesty, 0, 100);
  character_.honesty(static_cast<uint8_t>(new_honesty));
}

void Staff::adjust_health(int change) {
  int new_health = character_.health() + change;
  new_health = std::clamp(new_health, 0, 100);
  character_.health(static_cast<uint8_t>(new_health));
}

int Staff::pizzas_per_day() const {
  // Base productivity from character attributes
  int base = character_.pizzas_per_day();

  // Role-specific skill bonus
  int skill_bonus = 0;
  switch (job_role_) {
  case core::character::JobRole::CHEF:
    // Chefs: cooking skill influences pizza output
    skill_bonus = character_.cooking() / 2;
    break;
  case core::character::JobRole::WAITER:
    // Waiters: dexterity and serving skill influence service speed
    skill_bonus = (character_.dexterity() + character_.serving()) / 4;
    break;
  case core::character::JobRole::MANAGER:
    // Managers don't make pizzas
    [[fallthrough]];
  default:
    skill_bonus = 0;
    break;
  }

  // Total daily capacity
  int total = base + skill_bonus; // + payment_bonus;

  return total;
}

void Staff::pizzas_per_day(int ppd) { character_.pizzas_per_day(ppd); }

int Staff::calculate_prep_time_minutes() const {
  // Assume 12-hour operating day (720 minutes)
  // Time per pizza = total_minutes / pizzas_per_day
  constexpr int OPERATING_MINUTES_PER_DAY = 720;
  int ppd = pizzas_per_day();
  if (ppd == 0) {
    return OPERATING_MINUTES_PER_DAY; // Avoid division by zero
  }

  return OPERATING_MINUTES_PER_DAY / ppd;
}

int Staff::calculate_service_time_minutes() const {
  // Same calculation as prep time - converts throughput to time
  constexpr int OPERATING_MINUTES_PER_DAY = 720;
  int ppd = pizzas_per_day();
  if (ppd == 0) {
    return OPERATING_MINUTES_PER_DAY; // Avoid division by zero
  }

  return OPERATING_MINUTES_PER_DAY / ppd;
}

const core::character::Character& Staff::character() const {
  return character_;
}
core::character::JobRole Staff::job_role() const { return job_role_; }
