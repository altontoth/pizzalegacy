/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/StaffEvents.h"
#include "core/character/Staff.h"
#include "core/restaurant/Restaurant.h"
#include "engine/Random.h"

using namespace core::restaurant;
using engine::random::rand_percentage_chance;

void StaffEvents::daily_processing() {
  auto& cooks = restaurant_->cooks();
  auto& waiters = restaurant_->waiters();
  auto& manager = restaurant_->manager();
  for (auto& staff : cooks) {
    if (!staff) {
      continue;
    }
    process_staff_member(*staff);
  }
  for (auto& waiter : waiters) {
    if (!waiter) {
      continue;
    }
    process_staff_member(*waiter);
  }
  if (!manager) {
    return;
  }
  process_staff_member(*manager);
}

void StaffEvents::process_staff_member(Staff& staff) {
  // Check events in priority order
  if (check_training(staff)) {
    return;
  }
  if (check_late(staff)) {
    return;
  }
  if (check_sick(staff)) {
    return;
  }
  if (check_pregnancy(staff)) {
    return;
  }
  if (check_theft(staff)) {
    return;
  }
  // TODO: Add check_quitting, check_raise, check_disasters
}

bool StaffEvents::check_training(Staff& staff) {
  // TODO: Check if staff member is returning from training
  // For now, no training completion
  return false;
}

bool StaffEvents::check_late(Staff& staff) {
  // Trustworthiness% chance to NOT be late
  if (rand_percentage_chance(staff.character().trustworthiness())) {
    return false; // Not late
  }

  // Also check motivation% chance to avoid
  if (rand_percentage_chance(staff.character().motivation())) {
    return false; // Not late
  }

  // Staff member is late!
  staff.set_late(true);
  return true;
}

bool StaffEvents::check_sick(Staff& staff) {
  // Base 90% chance for not being sick
  if (rand_percentage_chance(90)) {
    return false;
  }

  if (rand_percentage_chance(staff.character().health() + 20)) {
    return false; // Resisted illness
  }

  if (rand_percentage_chance(staff.character().motivation())) {
    return false; // Came in despite illness
  }

  // Staff member is sick!
  staff.set_sick(true);
  // TODO: Set days_remaining for illness duration (1-4 days)
  return true;
}

bool StaffEvents::check_pregnancy(Staff& staff) {
  // Check gender flag
  if (staff.character().gender_flag() == 0) {
    return false; // Cannot get pregnant
  }

  // Four probability checks (charisma-based)
  uint8_t charisma = staff.character().charisma();

  // First: 4% chance
  if (!rand_percentage_chance(4)) {
    return false;
  }
  // Second: 4% chance
  if (!rand_percentage_chance(4)) {
    return false;
  }

  // Third: charisma% chance
  if (!rand_percentage_chance(charisma)) {
    return false;
  }

  // Fourth: charisma% chance
  if (!rand_percentage_chance(charisma)) {
    return false;
  }

  // Pregnancy occurred!
  staff.set_pregnant(true);
  // TODO: Set days_remaining = 90 (maternity leave duration)
  return true;
}

bool StaffEvents::check_theft(Staff& staff) {
  // Based on staff_daily_start_events.asm:327
  // Step 1: 3% base chance for theft opportunity
  if (!rand_percentage_chance(3)) {
    return false;
  }

  // Step 2: (honesty + 20)% chance to prevent theft
  if (rand_percentage_chance(staff.character().honesty() + 20)) {
    return false; // Resisted temptation
  }

  // Step 3: Calculate stolen amount (random(0, salary) * 10)
  auto theft_amount = engine::random::rand_max_n(staff.payment().whole() * 10);
  // TODO: Check if player has enough money
  // TODO: Transfer funds from player

  // Theft occurred!
  staff.set_stole_money(true);
  staff.adjust_motivation(30); // Thief is happy!
  return true;
}
