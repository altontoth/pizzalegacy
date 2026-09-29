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
    switch (training.job()) {
    case core::character::JobRole::CHEF:
      character_.cooking(
          raise(character_.cooking(), training.score_increase()));
      break;
    case core::character::JobRole::WAITER:
      character_.serving(
          raise(character_.serving(), training.score_increase()));
      break;
    case core::character::JobRole::MANAGER:
      character_.management(
          raise(character_.management(), training.score_increase()));
      break;
    case core::character::JobRole::NONE:
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
                      .job = training.job(),
                      .course_name = training.name()};

  current_course_.reset();
  return result;
}
void Staff::payment(Money p) { payment_ = p; }
Money Staff::payment() const { return payment_; }

void Staff::motivation(int motivation) {
  int new_motivation = std::clamp(motivation, 0, 100);
  character_.motivation(static_cast<uint8_t>(new_motivation));
}

void Staff::adjust_motivation(int change) {
  motivation(character_.motivation() + change);
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

int Staff::pizzas_per_day() const { return character_.pizzas_per_day(); }

void Staff::pizzas_per_day(int ppd) { character_.pizzas_per_day(ppd); }

const core::character::Character& Staff::character() const {
  return character_;
}
core::character::JobRole Staff::job_role() const { return job_role_; }
