/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/geom/PxOffset.h"
template <typename Tag> struct TypedPoint {
  int x{0};
  int y{0};

  constexpr TypedPoint() = default;
  constexpr TypedPoint(int x, int y) noexcept : x(x), y(y) {}

  friend constexpr bool operator==(TypedPoint a, TypedPoint b) noexcept {
    return a.x == b.x && a.y == b.y;
  }
  friend constexpr bool operator!=(TypedPoint a, TypedPoint b) noexcept {
    return !(a == b);
  }

  constexpr TypedPoint operator+(PxOffset b) const noexcept {
    return TypedPoint{x + b.dx, y + b.dy};
  }
  constexpr TypedPoint operator-(PxOffset b) const noexcept {
    return TypedPoint{x - b.dx, y - b.dy};
  }

  bool operator==(TypedPoint other) const {
    return x == other.x && y == other.y;
  }
};
