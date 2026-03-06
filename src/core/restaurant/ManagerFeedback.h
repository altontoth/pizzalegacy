/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

class Restaurant;
class Staff;

namespace core::restaurant {

class ManagerFeedback {
public:
  ManagerFeedback(Restaurant* r);
  void daily_adjustments();

private:
  void apply_behavior_adjustments(Staff& staff);

  Restaurant* restaurant_;
};

} // namespace core::restaurant
