/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <cstdint>

#include "engine/geom/Spaces.h"

enum class ButtonPosition : uint8_t {
  TopLeft = 0,
  TopCenter,
  TopRight,
  MiddleLeft,
  MiddleCenter,
  MiddleRight,
  BottomLeft,
  BottomCenter,
  BottomRight,
};

enum class ButtonType : uint8_t { Square, Horizontal, Vertical };

namespace ButtonGeometry {

constexpr Size square_button_sz{54, 50};

[[nodiscard]] constexpr Size size(ButtonType t) noexcept {
  switch (t) {
  case ButtonType::Square:
    return {square_button_sz.w, square_button_sz.h};
  case ButtonType::Horizontal:
    return {square_button_sz.w * 2, square_button_sz.h};
  case ButtonType::Vertical:
    return {square_button_sz.w, square_button_sz.h * 2};
  }
  return {0, 0};
}

constexpr ViewPoint inner_offset{2, 2};

// Convert grid cell + type to a ViewRect relative to (0,0).
[[nodiscard]] constexpr ViewRect rect(ButtonType t, ButtonPosition p) noexcept {
  const Size s = size(t);
  const int idx = static_cast<int>(p);
  const int col = idx % 3;
  const int row = idx / 3;

  return {(col * square_button_sz.w) + (col * inner_offset.x),
          (row * square_button_sz.h) + (row * inner_offset.y), s.w, s.h};
}
} // namespace ButtonGeometry
