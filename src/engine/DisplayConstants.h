/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string_view>

namespace DisplayConstants {

inline constexpr int SCREEN_WIDTH = 640;
inline constexpr int SCREEN_HEIGHT = 400;

inline constexpr int LOGICAL_HEIGHT = 400;
inline constexpr int CRT_DISPLAY_HEIGHT = 480; // 640x480 VGA mode

inline constexpr int BPP = 32;

} // namespace DisplayConstants

// Ticks a screen fade takes in one direction, so a round trip is twice this.
inline constexpr int default_fade_length = 12;

enum class ScreenResolution { Res_640x400, Res_1280x800 };

constexpr std::string_view to_string(ScreenResolution r) {
  switch (r) {
  case ScreenResolution::Res_640x400:
    return "640x400";
  case ScreenResolution::Res_1280x800:
    return "1280x800";
  }
  return "Unknown";
}

constexpr std::string_view to_config_value(ScreenResolution r) {
  return to_string(r);
}

inline std::optional<ScreenResolution>
screen_resolution_from_config_value(std::string_view value) {
  if (value == "640x400") {
    return ScreenResolution::Res_640x400;
  }
  if (value == "1280x800") {
    return ScreenResolution::Res_1280x800;
  }
  return std::nullopt;
}

constexpr int resolution_scale(ScreenResolution r) {
  switch (r) {
  case ScreenResolution::Res_640x400:
    return 1;
  case ScreenResolution::Res_1280x800:
    return 2;
  }
  return 1;
}

inline std::pair<int, int> resolution_window_size(ScreenResolution r,
                                                  int display_height) {
  int scale = resolution_scale(r);
  return {DisplayConstants::SCREEN_WIDTH * scale, display_height * scale};
}
