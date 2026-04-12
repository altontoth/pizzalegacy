/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <string>
#include "engine/FormatCompat.h"

namespace core::time {

/**
 * Represents a time of day (hour and minute).
 * Used for restaurant operating hours, appointments, etc.
 */
struct TimeOfDay {
  uint8_t hour{0};   // 0-23
  uint8_t minute{0}; // 0, 10, 20, 30, 40, 50 (10-minute increments)

  constexpr TimeOfDay() = default;
  constexpr TimeOfDay(int h, int m) : hour(h), minute(m) {}

  // Convert to block number within a day (0-143).
  // Each block is 10 minutes: 6 blocks per hour x 24 hours = 144 blocks.
  constexpr int to_block() const { return (hour * 6) + (minute / 10); }

  // Create TimeOfDay from block number (0-143).
  static constexpr TimeOfDay from_block(int block) {
    return TimeOfDay{block / 6, (block % 6) * 10};
  }

  // Calculate number of blocks between two times (same day).
  // Returns 0 if end <= start.
  static constexpr int blocks_between(TimeOfDay start, TimeOfDay end) {
    int start_block = start.to_block();
    int end_block = end.to_block();
    if (end_block <= start_block) {
      return 0;
    }
    return end_block - start_block;
  }

  // HH:MM
  std::string to_string() const {
    return compat::format("{:02}:{:02}", hour, minute);
  }

  constexpr auto operator<=>(const TimeOfDay&) const = default;
};

} // namespace core::time

// {} -> "HH:MM"  (e.g. "09:30")
template <> struct FORMAT_NAMESPACE::formatter<core::time::TimeOfDay> {
  constexpr auto parse(FORMAT_NAMESPACE::format_parse_context& ctx) {
    return ctx.begin();
  }

  template <typename Ctx>
  auto format(const core::time::TimeOfDay& t, Ctx& ctx) const {
    return FORMAT_NAMESPACE::format_to(ctx.out(), "{:02}:{:02}", t.hour,
                                       t.minute);
  }
};
