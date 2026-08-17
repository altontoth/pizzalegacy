/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
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

  constexpr GameDate(int y, int m, int d)
      : ymd_(std::chrono::year{y},
             std::chrono::month{static_cast<unsigned int>(m)},
             std::chrono::day{static_cast<unsigned int>(d)}) {}

  explicit constexpr GameDate(std::chrono::year_month_day d) : ymd_(d) {}

  uint32_t day() const { return unsigned(ymd_.day()); }

  uint32_t month() const { return unsigned(ymd_.month()); }

  int year() const { return int(ymd_.year()); }

  int year_short() const { return int(ymd_.year()) - 1900; }

  std::string to_string() const {
    auto y = int(ymd_.year());
    auto m = unsigned(ymd_.month());
    auto d = unsigned(ymd_.day());
    return compat::format("{}-{}-{}", d, short_month_names[m - 1], y);
  }

  // Compact CV format: "d-m-yy" (e.g. "1-3-95")
  std::string cv_date_string() const {
    return compat::format("{}-{}-{:02}", unsigned(ymd_.day()),
                          unsigned(ymd_.month()), year_short());
  }

  int days_since(const GameDate& other) const {
    return (std::chrono::sys_days{ymd_} - std::chrono::sys_days{other.ymd_})
        .count();
  }

  GameDate add_days(int days) const {
    return GameDate{std::chrono::year_month_day{std::chrono::sys_days{ymd_} +
                                                std::chrono::days{days}}};
  }

  constexpr GameDate add_years(int years) const {
    return GameDate{
        std::chrono::year_month_day{ymd_ + std::chrono::years{years}}};
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

  // English ordinal suffix for the day: "st", "nd", "rd", or "th"
  std::string_view ordinal_suffix() const {
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

  bool is_sunday() const { return day_of_week() == 7; }
  bool is_saturday() const { return day_of_week() == 6; }
  // The original's week is just a running 7-day counter from game start, not
  // the calendar weekday: index 0 is the first day of the week, 6 is end of
  // week.
  int week_day_index() const;
  bool is_end_of_week() const { return week_day_index() == 6; }
  bool is_end_of_month() const { return *this == month_end(); }
  bool is_end_of_year() const { return month() == 12 && day() == 31; }
  GameDate start_of_year() const;

private:
  std::chrono::year_month_day ymd_;
};

inline constexpr GameDate game_start_date{1995, 3, 1};

inline constexpr GameDate floor_editing_bonus_date =
    game_start_date.add_years(1);

inline int GameDate::week_day_index() const {
  return days_since(game_start_date) % 7;
}

inline GameDate GameDate::start_of_year() const {
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
