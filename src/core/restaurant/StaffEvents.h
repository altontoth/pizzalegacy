/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

class Restaurant;
class Staff;

namespace core::restaurant {

class StaffEvents {
public:
  explicit StaffEvents(Restaurant* r) : restaurant_(r) {}
  void daily_processing();

private:
  void process_staff_member(Staff& staff);

  bool check_training(Staff& staff);
  bool check_late(Staff& staff);
  bool check_sick(Staff& staff);
  bool check_pregnancy(Staff& staff);
  bool check_theft(Staff& staff);

  Restaurant* restaurant_;
};
} // namespace core::restaurant
