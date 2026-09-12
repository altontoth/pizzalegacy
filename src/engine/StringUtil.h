/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <string_view>

namespace util {

// Compare two strings ignoring case.
inline bool iequals(std::string_view a, std::string_view b) {
  return std::ranges::equal(a, b, [](unsigned char x, unsigned char y) {
    return std::tolower(x) == std::tolower(y);
  });
}

inline std::string_view trim_front(std::string_view sv) {
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
    sv.remove_prefix(1);
  }
  return sv;
}

inline std::string_view trim_back(std::string_view sv) {
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
    sv.remove_suffix(1);
  }
  return sv;
}

inline std::string_view trim(std::string_view sv) {
  return trim_back(trim_front(sv));
}

} // namespace util
