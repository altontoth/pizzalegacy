/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <string>

namespace colors {
struct Color {
  uint8_t r, g, b;

  constexpr Color(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0)
      : r(red), g(green), b(blue) {}

  std::string to_string() const {
    return std::string("r : " + std::to_string(r) + ", g: " +
                       std::to_string(g) + ", b: " + std::to_string(b));
  }

  constexpr bool operator==(const Color& other) const {
    return r == other.r && g == other.g && b == other.b;
  }

  constexpr bool operator!=(const Color& other) const {
    return !(*this == other);
  }
};
} // namespace colors
