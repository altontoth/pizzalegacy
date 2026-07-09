/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
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
  // daily_processing (events handled by a manager not included).
  std::vector<StaffEventData> take_pending_dialogs() {
    return std::move(pending_dialogs_);
  }

  // Apply the immediate effect. For theft it's the complete effect (money and
  // status update), for PERSF it's the change that happens before the dialog,
  // for PERS dialog events (late/sick) this is a no-op because their effects
  // depend on the user response in the popup and is handled by apply_outcome.
  static void apply_event_effect(StaffEventData::Type type, Staff& staff,
                                 Restaurant& restaurant, GameState& game_state);

  // Result of applying a player-chosen dialog outcome.
  struct Outcome {
    bool staff_left{false};
    // Staff reply line to show before the dialog chain finishes, if any.
    std::optional<std::string> reply;
  };

  // Apply the outcome-dependent effect for a dialog event (motivation
  // change, raise negotiation, firing). outcome is which of the three response
  // buttons the player picked (0-2). Returns nullopt when the staff member
  // doesn't exist anymore.
  static std::optional<Outcome> apply_outcome(const StaffEventData& event,
                                              int outcome,
                                              Restaurant& restaurant);

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
