/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <vector>
#include "core/character/Character.h"
#include "core/restaurant/StaffEventData.h"

class GameState;
class Restaurant;
class Staff;

namespace core::restaurant {

class StaffEvents {
public:
  StaffEvents(Restaurant* r, GameState* gs) : restaurant_(r), game_state_(gs) {}
  void daily_processing();

  // Returns (and clears) player-facing events collected during
  // daily_processing. Manager-auto-handled events are not included
  std::vector<StaffEventData> take_pending_dialogs() {
    return std::move(pending_dialogs_);
  }

  // Apply the immediate game-world effect for the given event type. For
  // Theft this is the complete effect (money transfer + status messages). For
  // PERSF types this is the change that happens before the dialog. For
  // PERS dialog events (Late/Sick/etc.) this is a no-op because their effects
  // are outcome-dependent and handled inside show_staff_event_dialog.
  static void apply_event_effect(StaffEventData::Type type, Staff& staff,
                                 Restaurant& restaurant, GameState& game_state);

private:
  void process_staff_member(Staff& staff, core::character::JobRole role,
                            int slot);

  bool check_late(Staff& staff, const StaffEventData& base);
  bool check_sick(Staff& staff, const StaffEventData& base);
  bool check_maternity(Staff& staff, const StaffEventData& base);
  bool check_salary(Staff& staff);
  bool check_quitting(Staff& staff, const StaffEventData& base);
  bool check_raise(Staff& staff, const StaffEventData& base);
  bool check_theft(Staff& staff);
  bool check_persf(Staff& staff, const StaffEventData& base);

  Restaurant* restaurant_;
  GameState* game_state_;
  bool event_fired_{false};
  std::vector<StaffEventData> pending_dialogs_;
};

} // namespace core::restaurant
