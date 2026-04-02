/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "core/character/Character.h"

class Staff;

namespace core::restaurant {

struct StaffEventData {
  enum class Type {
    Late,
    Sick,
    Maternity,
    Quit,
    Raise,
    Theft,
    DestroyCheeseStock,
    BlowUpOven,
    SpillWine,
    MessedUpOrders,
    ForgotToOpen,
  };

  Staff* staff{nullptr};
  core::character::JobRole job_role{core::character::JobRole::NONE};
  int slot_index{-1};
  Type type{Type::Late};
};

} // namespace core::restaurant
