/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <utility>

namespace DisplayConstants {

inline constexpr int SCREEN_WIDTH = 640;
inline constexpr int SCREEN_HEIGHT = 400;

inline constexpr int LOGICAL_HEIGHT = 400;
inline constexpr int CRT_DISPLAY_HEIGHT = 480; // 640x480 VGA mode

inline constexpr int BPP = 32;

} // namespace DisplayConstants

// Ticks a screen fade takes in one direction, so a round trip is twice this.
inline constexpr int default_fade_length = 12;

inline std::pair<int, int> window_size_for(bool double_size,
                                           int display_height) {
  const int scale = double_size ? 2 : 1;
  return {DisplayConstants::SCREEN_WIDTH * scale, display_height * scale};
}
