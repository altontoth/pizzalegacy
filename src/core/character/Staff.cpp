/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/character/Staff.h"

#include "engine/Random.h"

#include <algorithm>

Staff::Staff(core::character::JobRole job_role,
             core::character::Character character)
    : job_role_(job_role), character_(character) {}

int Staff::time_until_back() const { return days_away_remaining_; }

bool Staff::is_away() const {
  return days_away_remaining_ > 0 || current_course_.has_value();
}

void Staff::days_away(int days) { days_away_remaining_ = days; }

void Staff::tick_days_away() {
  if (days_away_remaining_ <= 0) {
    return;
  }
  --days_away_remaining_;
  if (days_away_remaining_ == 0) {
    is_sick_ = false;
    is_pregnant_ = false;
    // course completion happens elsewhere
  }
}
void Staff::send_on_training(const Training& training) {
  current_course_ = training;
  days_away_remaining_ = training.duration();
}

CourseResult Staff::complete_course() {
  const Training& training = *current_course_;
  const int index = training.course_index();

  // Courses 0 and 1 always succeed; courses 2-5 pass with (motivation + 20)%.
  bool success = index <= 1 || engine::random::rand_percentage_chance(
                                   character_.motivation() + 20);

  if (success) {
    auto raise = [](uint8_t current, int gain) {
      return static_cast<uint8_t>(std::clamp(current + gain, 0, 100));
    };
    switch (training.type()) {
    case SkillType::COOKING:
      character_.cooking(
          raise(character_.cooking(), training.score_increase()));
      break;
    case SkillType::SERVING:
      character_.serving(
          raise(character_.serving(), training.score_increase()));
      break;
    case SkillType::MANAGEMENT:
      character_.management(
          raise(character_.management(), training.score_increase()));
      break;
    case SkillType::NONE:
      break;
    }
    character_.pizzas_per_day(character_.pizzas_per_day() +
                              training.pizzas_day_increase());
  }

  // Attending raises intelligence even on failure
  int intelligence_gain = 4 + engine::random::rand_max_n(index + 1);
  character_.intelligence(static_cast<uint8_t>(
      std::min(character_.intelligence() + intelligence_gain, 90)));

  CourseResult result{.success = success,
                      .mastered = success && index == 5,
                      .skill = training.type(),
                      .course_name = training.name()};

  current_course_.reset();
  return result;
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
    // Managers don't get a bonus
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
