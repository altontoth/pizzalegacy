/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include "engine/FormatCompat.h"

inline constexpr std::array<std::string_view, 12> month_names{
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December",
};

inline constexpr std::array<std::string_view, 12> short_month_names = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

// No leap years, just simple 365 day years.
inline constexpr std::array<int, 12> days_in_month{31, 28, 31, 30, 31, 30,
                                                   31, 31, 30, 31, 30, 31};
inline constexpr int days_per_year = 365;

// Dates are stored as the number of days from the start of this year.
inline constexpr int calendar_epoch_year = 1900;

// Days from 1 January to the given date, counting the 1st as 0.
constexpr int day_of_year(int month, int day) {
  int total = day - 1;
  for (int m = 0; m < month - 1; ++m) {
    total += days_in_month[m];
  }
  return total;
}

class GameDate {
public:
  friend std::ostream& operator<<(std::ostream& os, const GameDate& date) {
    os << date.to_string();
    return os;
  }

  constexpr GameDate(int y, int m, int d)
      : days_(((y - calendar_epoch_year) * days_per_year) + day_of_year(m, d)) {
  }

  constexpr uint32_t day() const {
    return static_cast<uint32_t>(month_day().day);
  }

  constexpr uint32_t month() const {
    return static_cast<uint32_t>(month_day().month);
  }

  constexpr int year() const {
    return calendar_epoch_year + (days_ / days_per_year);
  }

  constexpr int year_short() const { return year() - 1900; }

  std::string to_string() const {
    return compat::format("{}-{}-{}", day(), short_month_names[month() - 1],
                          year());
  }

  // Compact CV format: "d-m-yy" (e.g. "1-3-95")
  std::string cv_date_string() const {
    return compat::format("{}-{}-{:02}", day(), month(), year_short());
  }

  constexpr int days_since(const GameDate& other) const {
    return days_ - other.days_;
  }

  constexpr GameDate add_days(int days) const {
    return GameDate{from_day_count, days_ + days};
  }

  constexpr GameDate add_years(int years) const {
    return add_days(years * days_per_year);
  }

  auto operator<=>(const GameDate&) const = default;

  constexpr GameDate month_start() const {
    auto md = month_day();
    return GameDate{year(), md.month, 1};
  }
  constexpr GameDate month_end() const {
    auto md = month_day();
    return GameDate{year(), md.month, days_in_month[md.month - 1]};
  }
  constexpr GameDate previous_month_start() const {
    return previous_month_end().month_start();
  }

  constexpr GameDate previous_month_end() const {
    return month_start().add_days(-1);
  }

  constexpr std::string_view month_name() const {
    return month_names[month() - 1];
  }

  constexpr std::string_view month_name_short() const {
    return short_month_names[month() - 1];
  }

  constexpr std::string_view previous_month_name() const {
    return month_names[previous_month_end().month() - 1];
  }

  // English ordinal suffix for the day: "st", "nd", "rd", or "th"
  constexpr std::string_view ordinal_suffix() const {
    auto d = day();
    if (d >= 11 && d <= 13) {
      return "th";
    }
    switch (d % 10) {
    case 1:
      return "st";
    case 2:
      return "nd";
    case 3:
      return "rd";
    default:
      return "th";
    }
  }

  // Day with ordinal suffix, e.g. "3rd", "21st"
  std::string day_with_suffix() const {
    return compat::format("{}{}", day(), ordinal_suffix());
  }

  // The original's week is just a running 7-day counter from game start, not
  // the calendar weekday: index 0 is the first day of the week, 6 is end of
  // week.
  constexpr int week_day_index() const;
  constexpr bool is_end_of_week() const { return week_day_index() == 6; }
  constexpr bool is_end_of_month() const { return *this == month_end(); }
  constexpr bool is_end_of_year() const { return month() == 12 && day() == 31; }
  constexpr GameDate start_of_year() const;

private:
  struct FromDayCount {};
  static constexpr FromDayCount from_day_count{};
  constexpr GameDate(FromDayCount, int days) : days_(days) {}

  // Month and day of this date, both 1-based.
  struct MonthDay {
    int month;
    int day;
  };
  constexpr MonthDay month_day() const {
    int remaining = days_ - ((year() - calendar_epoch_year) * days_per_year);
    int m = 0;
    while (m < 11 && remaining >= days_in_month[m]) {
      remaining -= days_in_month[m];
      ++m;
    }
    return {m + 1, remaining + 1};
  }

  int days_;
};

inline constexpr GameDate game_start_date{1995, 3, 1};

inline constexpr GameDate floor_editing_bonus_date =
    game_start_date.add_years(1);

constexpr int GameDate::week_day_index() const {
  return days_since(game_start_date) % 7;
}

constexpr GameDate GameDate::start_of_year() const {
  return std::max(GameDate{year(), 1, 1}, game_start_date);
}

// {} or {:d} -> "1-Mar-1995"  (standard, matches to_string())
// {:c}       -> "1-3-95"      (compact cv format, matches cv_date_string())
template <> struct FORMAT_NAMESPACE::formatter<GameDate> {
  enum class style { standard, compact };
  style how{style::standard};

  constexpr auto parse(FORMAT_NAMESPACE::format_parse_context& ctx) {
    auto it = ctx.begin();
    if (it != ctx.end() && *it != '}') {
      switch (*it) {
      case 'd':
        how = style::standard;
        break;
      case 'c':
        how = style::compact;
        break;
      default:
        throw FORMAT_NAMESPACE::format_error("invalid GameDate format");
      }
      ++it;
    }
    if (it != ctx.end() && *it != '}') {
      throw FORMAT_NAMESPACE::format_error("expected '}'");
    }
    return it;
  }

  template <typename Ctx> auto format(const GameDate& d, Ctx& ctx) const {
    if (how == style::compact) {
      return FORMAT_NAMESPACE::format_to(ctx.out(), "{}", d.cv_date_string());
    }
    return FORMAT_NAMESPACE::format_to(ctx.out(), "{}", d.to_string());
  }
};
