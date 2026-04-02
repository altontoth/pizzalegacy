/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/ManagerFeedback.h"
#include "core/restaurant/Restaurant.h"
#include "engine/Random.h"

using namespace core::restaurant;

ManagerFeedback::ManagerFeedback(Restaurant* r) : restaurant_(r) {}

int ManagerFeedback::calculate_harshness(ManagerBehavior behavior) {
  if (behavior == ManagerBehavior::Warmly) {
    behavior = ManagerBehavior::Friendly;
  }

  int b = static_cast<int>(behavior);
  return (5 - b) >> 1;
}

void ManagerFeedback::daily_adjustments() {
  if (!restaurant_->manager()) {
    return;
  }

  for (auto& cook : restaurant_->cooks()) {
    if (!cook) {
      break;
    }
    apply_behavior_adjustments(*cook);
  }
  for (auto& waiter : restaurant_->waiters()) {
    if (!waiter) {
      break;
    }
    apply_behavior_adjustments(*waiter);
  }
}

void ManagerFeedback::apply_behavior_adjustments(Staff& staff) {
  auto& manager = restaurant_->manager();
  if (!manager) {
    return;
  }

  const auto& mgr_char = manager->character();
  const auto& staff_char = staff.character();

  switch (restaurant_->manager_settings().behavior_towards_staff) {
  case ManagerBehavior::Merciless:
    staff.adjust_motivation(-3);
    [[fallthrough]];

  case ManagerBehavior::Severe:
    if (staff_char.trustworthiness() < staff_char.experience() + 10) {
      staff.adjust_trustworthiness(2);
    }
    staff.adjust_motivation(-1);
    break;

  case ManagerBehavior::Strict: {
    if (engine::random::rand_percentage_chance(mgr_char.personality())) {
      if (staff_char.health() < staff_char.motivation() + 10) {
        staff.adjust_health(1);
      }
    }

    if (staff_char.trustworthiness() < staff_char.experience()) {
      if (engine::random::rand_percentage_chance(mgr_char.personality())) {
        staff.adjust_trustworthiness(1);
      }
    }
    break;
  }

  case ManagerBehavior::Moderate:
    if (staff_char.motivation() < 80) {
      if (engine::random::rand_percentage_chance(mgr_char.charisma())) {
        staff.adjust_motivation(1);
      }
    }
    break;

  case ManagerBehavior::Cooperative: {
    if (mgr_char.personality() > 50) {
      if (staff_char.motivation() < staff_char.health() + 10) {
        staff.adjust_motivation(1);
      }
    }

    if (staff_char.honesty() < staff_char.intelligence() + 15) {
      staff.adjust_honesty(1);
    }
    break;
  }

  case ManagerBehavior::Friendly:
    if (staff_char.motivation() > 8) {
      staff.adjust_motivation(-1);
    }

    if (staff_char.honesty() > staff_char.trustworthiness() + 8) {
      staff.adjust_honesty(-1);
    }

    [[fallthrough]];

  case ManagerBehavior::Warmly:
    // If trustworthiness > motivation + 10: -1 trustworthiness
    if (staff_char.trustworthiness() > staff_char.motivation() + 10) {
      staff.adjust_trustworthiness(-1);
    }
    break;
  }
}
