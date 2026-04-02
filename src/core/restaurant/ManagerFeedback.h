/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/restaurant/Restaurant.h"

class Staff;

namespace core::restaurant {

class ManagerFeedback {
public:
  ManagerFeedback(Restaurant* r);
  void daily_adjustments();

  // Compute manager harshness from behavior setting (0-2).
  // Used by StaffEvents to determine how the manager handles events.
  static int calculate_harshness(ManagerBehavior behavior);

private:
  void apply_behavior_adjustments(Staff& staff);

  Restaurant* restaurant_;
};

} // namespace core::restaurant
