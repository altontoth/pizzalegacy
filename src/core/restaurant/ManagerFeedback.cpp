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
    // -3 motivation, then fall through to Severe
    staff.adjust_motivation(-3);
    [[fallthrough]];

  case ManagerBehavior::Severe:
    // If trustworthiness < experience + 10: +2 trustworthiness
    if (staff_char.trustworthiness() < staff_char.experience() + 10) {
      staff.adjust_trustworthiness(2);
    }
    // Then -1 motivation
    staff.adjust_motivation(-1);
    break;

  case ManagerBehavior::Strict: {
    // (Manager personality)% chance: If health < motivation + 10, +1 health
    if (engine::random::rand_percentage_chance(mgr_char.personality())) {
      if (staff_char.health() < staff_char.motivation() + 10) {
        staff.adjust_health(1);
      }
    }

    // If trustworthiness < experience: (Manager personality)% chance to +1
    // trustworthiness
    if (staff_char.trustworthiness() < staff_char.experience()) {
      if (engine::random::rand_percentage_chance(mgr_char.personality())) {
        staff.adjust_trustworthiness(1);
      }
    }
    break;
  }

  case ManagerBehavior::Moderate:
    // If motivation < 80: (Manager charisma)% chance to +1 motivation
    if (staff_char.motivation() < 80) {
      if (engine::random::rand_percentage_chance(mgr_char.charisma())) {
        staff.adjust_motivation(1);
      }
    }
    break;

  case ManagerBehavior::Cooperative: {
    // If manager personality > 50 AND motivation < health + 10: +1 motivation
    if (mgr_char.personality() > 50) {
      if (staff_char.motivation() < staff_char.health() + 10) {
        staff.adjust_motivation(1);
      }
    }

    // If honesty < intelligence + 15: +1 honesty
    if (staff_char.honesty() < staff_char.intelligence() + 15) {
      staff.adjust_honesty(1);
    }
    break;
  }

  case ManagerBehavior::Friendly:
    // If motivation > 8: -1 motivation
    if (staff_char.motivation() > 8) {
      staff.adjust_motivation(-1);
    }

    // If honesty > trustworthiness + 8: -1 honesty
    if (staff_char.honesty() > staff_char.trustworthiness() + 8) {
      staff.adjust_honesty(-1);
    }

    // Fall through to Warmly
    [[fallthrough]];

  case ManagerBehavior::Warmly:
    // If trustworthiness > motivation + 10: -1 trustworthiness
    if (staff_char.trustworthiness() > staff_char.motivation() + 10) {
      staff.adjust_trustworthiness(-1);
    }
    break;
  }
}
