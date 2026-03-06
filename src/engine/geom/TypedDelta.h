/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

template <typename Tag> struct TypedDelta {
  int dx{0};
  int dy{0};

  constexpr TypedDelta() = default;
  constexpr TypedDelta(int dx, int dy) noexcept : dx(dx), dy(dy) {}

  friend constexpr bool operator==(TypedDelta a, TypedDelta b) noexcept {
    return a.dx == b.dx && a.dy == b.dy;
  }
  friend constexpr bool operator!=(TypedDelta a, TypedDelta b) noexcept {
    return !(a == b);
  }
};
