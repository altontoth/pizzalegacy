/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <chrono>
#include <string>
#include "engine/FormatCompat.h"

inline constexpr std::array<std::string_view, 12> month_names{
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December",
};

inline constexpr std::array<std::string_view, 12> short_month_names = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

class GameDate {
public:
  friend std::ostream& operator<<(std::ostream& os, const GameDate& date) {
    os << date.to_string();
    return os;
  }

  GameDate(int y, int m, int d)
      : ymd_(std::chrono::year{y},
             std::chrono::month{static_cast<unsigned int>(m)},
             std::chrono::day{static_cast<unsigned int>(d)}) {}

  explicit GameDate(std::chrono::year_month_day d) : ymd_(d) {}

  uint32_t day() const { return unsigned(ymd_.day()); }

  uint32_t month() const { return unsigned(ymd_.month()); }

  uint32_t year() const { return int(ymd_.year()); }

  uint32_t year_short() const { return int(ymd_.year()) - 1900; }

  std::string to_string() const {
    auto y = int(ymd_.year());
    auto m = unsigned(ymd_.month());
    auto d = unsigned(ymd_.day());
    return compat::format("{}-{}-{}", d, short_month_names[m - 1], y);
  }

  int days_since(const GameDate& other) const {
    return (std::chrono::sys_days{ymd_} - std::chrono::sys_days{other.ymd_})
        .count();
  }

  GameDate add_days(int days) const {
    return GameDate{std::chrono::year_month_day{std::chrono::sys_days{ymd_} +
                                                std::chrono::days{days}}};
  }

  auto operator<=>(const GameDate&) const = default;

  GameDate month_start() const {
    return GameDate{std::chrono::year_month_day{ymd_.year(), ymd_.month(),
                                                std::chrono::day{1}}};
  }
  GameDate month_end() const {
    auto last_day = std::chrono::year_month_day_last{
        ymd_.year(), std::chrono::month_day_last{ymd_.month()}};
    return GameDate{std::chrono::year_month_day{last_day}};
  }
  GameDate previous_month_start() const {
    auto prev = std::chrono::year_month_day{ymd_.year(), ymd_.month(),
                                            std::chrono::day{1}} -
                std::chrono::months{1};
    return GameDate{std::chrono::year_month_day{prev.year(), prev.month(),
                                                std::chrono::day{1}}};
  }

  GameDate previous_month_end() const {
    auto prev_month = ymd_ - std::chrono::months{1};
    auto last_day = std::chrono::year_month_day_last{
        prev_month.year(), std::chrono::month_day_last{prev_month.month()}};
    return GameDate{std::chrono::year_month_day{last_day}};
  }

  std::string_view month_name() const {
    unsigned m = static_cast<unsigned>(ymd_.month());
    return (m >= 1 && m <= 12) ? month_names[m - 1] : "Invalid";
  }

  std::string_view month_name_short() const {
    unsigned m = static_cast<unsigned>(ymd_.month());
    return (m >= 1 && m <= 12) ? short_month_names[m - 1] : "N/A";
  }

  std::string_view previous_month_name() const {
    auto prev_month = ymd_.month() - std::chrono::months{1};
    return month_names[static_cast<unsigned>(prev_month) - 1];
  }

  // Day of week (1 = Monday, 7 = Sunday)
  unsigned day_of_week() const {
    auto dp = std::chrono::sys_days{ymd_};
    auto wd = std::chrono::weekday{dp};
    return wd.iso_encoding(); // 1=Monday, 7=Sunday
  }

  bool is_sunday() const { return day_of_week() == 7; }
  bool is_saturday() const { return day_of_week() == 6; }
  bool is_end_of_week() const { return is_saturday(); }
  bool is_end_of_month() const { return *this == month_end(); }
  bool is_end_of_year() const { return month() == 12 && day() == 31; }

private:
  std::chrono::year_month_day ymd_;
};

inline const GameDate game_start_date{1995, 3, 1};
/*

  // Game starts on 1st of March 1995
  int year = 1995;
  int month = 3;
  int day = 1;

  bool operator==(const GameDate&) const = default;
  std::strong_ordering operator<=>(const GameDate& other) const;

  // Month name methods
  std::string month_name() const;
  std::string previous_month_name() const;
  int previous_month() const;
  std::string to_string() const;
  static bool is_leap_year(int year);
  static int days_in_month(int month, int year);

  // Helpers for date ranges
  GameDate month_start() const;
  GameDate month_end() const;
  GameDate previous_month_start() const;
  GameDate previous_month_end() const;

private:
  static constexpr std::array<std::string_view, 12> month_names = {
      "January", "February", "March",     "April",   "May",      "June",
      "July",    "August",   "September", "October", "November", "December",
  };
};

int operator-(const GameDate&, const GameDate&);
*/
