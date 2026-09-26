/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "core/character/Character.h"

namespace core::character {

enum class MarketStatus {
  CHEF = 0,
  WAITER = 1,
  MANAGER = 2,
  RESERVE_CHEF = 3,
  RESERVE_WAITER = 4,
  RESERVE_MANAGER = 5,
  HIRED_BASE = 8,   // hired by player N: this + N
  PLAYER_BASE = 16, // Player char of player N: this + N
};

inline MarketStatus hired_status(int player_index) {
  return static_cast<MarketStatus>(
      static_cast<uint8_t>(MarketStatus::HIRED_BASE) + player_index);
}

inline MarketStatus player_status(int player_index) {
  return static_cast<MarketStatus>(
      static_cast<uint8_t>(MarketStatus::PLAYER_BASE) + player_index);
}

inline bool is_available(MarketStatus s) { return static_cast<uint8_t>(s) < 3; }

inline bool is_reserve(MarketStatus s) {
  auto v = static_cast<uint8_t>(s);
  return v >= 3 && v < 6;
}

inline bool is_hired(MarketStatus s) {
  return static_cast<uint8_t>(s) >=
             static_cast<int>(MarketStatus::HIRED_BASE) &&
         static_cast<uint8_t>(s) < static_cast<int>(MarketStatus::PLAYER_BASE);
}

inline bool is_hired_or_player(MarketStatus s) {
  return static_cast<uint8_t>(s) >= 6;
}

inline JobRole job_role(MarketStatus s) {
  return static_cast<JobRole>(static_cast<uint8_t>(s) % 3);
}

inline MarketStatus to_reserve(MarketStatus s) {
  return static_cast<MarketStatus>(static_cast<uint8_t>(s) + 3);
}

inline MarketStatus from_reserve(MarketStatus s) {
  return static_cast<MarketStatus>(static_cast<uint8_t>(s) - 3);
}

class JobMarket {
public:
  static constexpr int ORIGINAL_SLOTS = 100;

  int total_slots() const { return ORIGINAL_SLOTS + double_slots_; }

  // Reads chr/amarkt and t/double.e to determine slot counts, pre-seeds
  // originals, initialises doubles as reserve with the correct job type,
  // then marks each player's starting character.
  // player_chars: list of {character_id, player_index} pairs
  void init(const std::vector<std::pair<uint8_t, int>>& player_chars);

  // Weekly rotation: probabilistically moves characters between
  // available and reserve to maintain a target pool size per job type.
  void end_of_week(int difficulty);

  MarketStatus status(int id) const { return slots_[id]; }
  void set_status(int id, MarketStatus value) { slots_[id] = value; }

  const std::vector<MarketStatus>& slots() const { return slots_; }
  void load_slots(const std::vector<MarketStatus>& slots) {
    slots_ = slots;
    double_slots_ = static_cast<int>(slots.size()) - ORIGINAL_SLOTS;
  }

private:
  int double_slots_ = 0;
  std::vector<MarketStatus> slots_;
};

} // namespace core::character
