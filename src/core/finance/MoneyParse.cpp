/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/finance/MoneyParse.h"

namespace core::finance::parse {

// Parses "12.34", "12,34", "12", "  € 12,3 " -> Money.
// Ignores thousands sep (we treat all non-digits except one
// final '.'/',' as ignorable).
std::optional<Money> from_input(std::string_view sv) {
  // find last '.' or ',' => decimal separator (common EU/US handling)
  auto last_dot = sv.find_last_of('.');
  auto last_comma = sv.find_last_of(',');
  size_t sep_pos = std::string_view::npos;
  if (last_dot != std::string_view::npos ||
      last_comma != std::string_view::npos) {
    sep_pos = (last_dot == std::string_view::npos) ? last_comma
              : (last_comma == std::string_view::npos)
                  ? last_dot
                  : std::max(last_dot, last_comma);
  }

  auto is_digit = [](char c) { return c >= '0' && c <= '9'; };

  // accumulate integer part
  long long whole = 0;
  bool any_int = false;
  for (size_t i = 0; i < sv.size(); ++i) {
    if (i == sep_pos) {
      break;
    }
    char c = sv[i];
    if (is_digit(c)) {
      whole = whole * 10 + (c - '0');
      any_int = true;
    }
  }

  // accumulate fractional part (max 2 digits)
  int cents = 0;
  int frac_digits = 0;
  if (sep_pos != std::string_view::npos) {
    for (size_t i = sep_pos + 1; i < sv.size() && frac_digits < 2; ++i) {
      char c = sv[i];
      if (!is_digit(c)) {
        continue;
      }
      cents = cents * 10 + (c - '0');
      ++frac_digits;
    }
  }
  if (frac_digits == 0) {
    cents = 0;
  }
  else if (frac_digits == 1) {
    cents *= 10; // "12,3" -> "12,30"
  }

  if (!any_int && frac_digits == 0) {
    return std::nullopt; // no digits at all
  }
  long long total = (whole * 100) + cents;
  if (total > std::numeric_limits<int>::max()) {
    return std::nullopt;
  }
  return Money{total};
}

} // namespace core::finance::parse
