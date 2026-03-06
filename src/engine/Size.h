/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "FormatCompat.h"

struct Size {
  int w = 0;
  int h = 0;

  constexpr Size() : w(0), h(0) {}
  constexpr Size(int new_w, int new_h) : w(new_w), h(new_h) {}
};

constexpr Size operator*(const Size& a, const Size& b) {
  return Size{a.w * b.w, a.h * b.h};
}

constexpr Size operator*(const Size& a, int factor) {
  return Size{a.w * factor, a.h * factor};
}

constexpr Size operator+(const Size& a, const Size& b) {
  return Size{a.w + b.w, a.h + b.h};
}

constexpr Size operator-(const Size& a, const Size& b) {
  return Size{a.w - b.w, a.h - b.h};
}

inline std::string to_string(const Size& s) {
  return compat::format("({}, {})", s.w, s.h);
}

// formatter specialization
template <>
struct FORMAT_NAMESPACE::formatter<Size>
    : FORMAT_NAMESPACE::formatter<std::string> {
  auto format(const Size& s, auto& ctx) const {
    return FORMAT_NAMESPACE::formatter<std::string>::format(
        FORMAT_NAMESPACE::format("({},{})", s.w, s.h), ctx);
  }
};
