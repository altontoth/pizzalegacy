/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <string>
#include "core/time/GameTime.h" // your existing GameTime/GameDate

namespace core::time {
struct GameTimestamp {
  GameDate date;         // absolute in-game date
  uint16_t block_in_day; // 0..143 (each = 10 minutes)

  uint8_t hour() const { return static_cast<uint8_t>(block_in_day / 6); }
  uint8_t minute() const {
    return static_cast<uint8_t>((block_in_day % 6) * 10);
  }

  std::string hhmm() const {
    return compat::format("{:02}:{:02}", hour(), minute());
  }
  std::string date_string() const { return date.to_string(); }
};

inline GameTimestamp make_stamp(const GameTime& t) {
  const int blocks = t.time_blocks();
  const int day_blocks = blocks % 144;
  return GameTimestamp{
      .date = t.current_date(),
      .block_in_day = static_cast<uint16_t>(day_blocks),
  };
}
} // namespace core::time
