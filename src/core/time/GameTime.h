/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include "core/time/GameDate.h"
#include "core/time/TimeOfDay.h"
#include "engine/FormatCompat.h"

enum class Season {
  Spring,
  Summer,
  Fall,
  Winter,
};

class GameTime {
public:
  static constexpr int BLOCKS_PER_DAY = 144;

  GameTime() : blocks_since_start_() {}

  void advance_10_minutes() { blocks_since_start_++; }

  void advance_blocks(int n) { blocks_since_start_ += n; }

  void next_day() { blocks_since_start_ += BLOCKS_PER_DAY; }

  GameDate current_date() const {
    return game_start_date.add_days(blocks_since_start_ / BLOCKS_PER_DAY);
  }

  std::string to_date_string() const { return current_date().to_string(); }

  std::string to_time_string() const {
    int remaining_blocks = blocks_since_start_ % BLOCKS_PER_DAY;
    int hour = remaining_blocks / 6;
    int minute = (remaining_blocks % 6) * 10;
    return compat::format("{}:{:02}", hour, minute);
  }

  std::string to_string() const {
    return compat::format("{}, {}", to_date_string(), to_time_string());
  }

  int time_blocks() const { return blocks_since_start_; }
  void time_blocks(int blocks) { blocks_since_start_ = blocks; }

  void time_on_next_day(int hour, int minute) {
    // Advance to start of next day
    blocks_since_start_ =
        ((blocks_since_start_ / BLOCKS_PER_DAY) + 1) * BLOCKS_PER_DAY;
    // Add offset for time
    blocks_since_start_ += (hour * 6) + (minute / 10);
  }

  void set_time_of_day(int hour, int minute) {
    // Keep the same day but change the time
    int day_blocks = (blocks_since_start_ / BLOCKS_PER_DAY) * BLOCKS_PER_DAY;
    int time_blocks = (hour * 6) + (minute / 10);
    blocks_since_start_ = day_blocks + time_blocks;
  }

  bool is_past_time(core::time::TimeOfDay t) const {
    return current_block_of_day() >= t.to_block();
  }

  static Season season_from_month(unsigned month) {
    if (month >= 3 && month <= 5) {
      return Season::Spring;
    }
    if (month >= 6 && month <= 8) {
      return Season::Summer;
    }
    if (month >= 9 && month <= 11) {
      return Season::Fall;
    }
    return Season::Winter;
  }

  static std::string to_string(Season s) {
    switch (s) {
    case Season::Spring: {
      return "SPRING";
    }
    case Season::Summer: {
      return "SUMMER";
    }
    case Season::Fall: {
      return "FALL";
    }
    case Season::Winter: {
      return "WINTER";
    }
    }
    return "UNKNOWN";
  }

  Season season() const {
    return season_from_month(static_cast<unsigned>(current_date().month()));
  }

  /**
   * Get current block within the day (0-143).
   */
  int current_block_of_day() const {
    return blocks_since_start_ % BLOCKS_PER_DAY;
  }

  int current_hour() const { return current_block_of_day() / 6; }
  int current_minute() const { return (current_block_of_day() % 6) * 10; }
  core::time::TimeOfDay time_of_day() const {
    return core::time::TimeOfDay{current_hour(), current_minute()};
  }

  // Calculate blocks remaining until a specific time today.
  // Returns 0 if already past that time.
  int blocks_until(int target_hour, int target_minute) const {
    int current = current_block_of_day();
    int target = (target_hour * 6) + (target_minute / 10);
    if (target <= current) {
      return 0; // Already past target time
    }
    return target - current;
  }

private:
  int blocks_since_start_;
};

// {}   -> "1-Mar-1995, 9:00"  (full date and time)
// {:d} -> "1-Mar-1995"        (date only)
// {:t} -> "9:00"              (time only)
template <> struct FORMAT_NAMESPACE::formatter<GameTime> {
  enum class style { full, date_only, time_only };
  style how{style::full};

  constexpr auto parse(FORMAT_NAMESPACE::format_parse_context& ctx) {
    auto it = ctx.begin();
    if (it != ctx.end() && *it != '}') {
      switch (*it) {
      case 'd':
        how = style::date_only;
        break;
      case 't':
        how = style::time_only;
        break;
      default:
        throw FORMAT_NAMESPACE::format_error("invalid GameTime format");
      }
      ++it;
    }
    if (it != ctx.end() && *it != '}') {
      throw FORMAT_NAMESPACE::format_error("expected '}'");
    }
    return it;
  }

  template <typename Ctx> auto format(const GameTime& t, Ctx& ctx) const {
    switch (how) {
    case style::date_only:
      return FORMAT_NAMESPACE::format_to(ctx.out(), "{}", t.to_date_string());
    case style::time_only:
      return FORMAT_NAMESPACE::format_to(ctx.out(), "{}", t.to_time_string());
    default:
      return FORMAT_NAMESPACE::format_to(ctx.out(), "{}", t.to_string());
    }
  }
};
