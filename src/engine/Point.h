/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "engine/FormatCompat.h"

struct Point {
  int x = 0;
  int y = 0;

  constexpr Point() : x(0), y(0) {}
  constexpr Point(int new_x, int new_y) : x(new_x), y(new_y) {}

  constexpr Point& operator+=(const Point& other) noexcept {
    x += other.x;
    y += other.y;
    return *this;
  }

  constexpr Point& operator-=(const Point& other) noexcept {
    x -= other.x;
    y -= other.y;
    return *this;
  }
};

constexpr Point operator+(Point a, const Point& b) { return a += b; }

constexpr Point operator-(Point a, const Point& b) { return a -= b; }

constexpr Point operator*(Point a, int b) { return Point{a.x * b, a.y * b}; }

template <>
struct FORMAT_NAMESPACE::formatter<Point>
    : FORMAT_NAMESPACE::formatter<std::string> {
  auto format(const Point& s, auto& ctx) const {
    return FORMAT_NAMESPACE::formatter<std::string>::format(
        FORMAT_NAMESPACE::format("({},{})", s.x, s.y), ctx);
  }
};
