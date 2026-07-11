/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>

#include "engine/DisplayConstants.h"
#include "resources/util/Platform.h"

// FPS is decoupled from the 30Hz tick, so a higher rate only smooths
// input/cursor/redraw.
enum class FpsOption : int { Fps30 = 30, Fps60 = 60, Fps120 = 120 };

inline std::string to_string(FpsOption fps) {
  switch (fps) {
  case FpsOption::Fps30:
    return "30";
  case FpsOption::Fps60:
    return "60";
  case FpsOption::Fps120:
    return "120";
  }
  return "??";
}

struct GameOptions {
  bool map_fx = true;
  bool restaurant_fx = true;
  bool music = true;
  float sound_volume = 0.9f;
  float music_volume = 1.0f;
  bool automatic_report = false;
  // Use 640x480 (4:3) CRT aspect ratio instead of 640x400
  bool crt_aspect_ratio = false;
  bool fix_legacy_bugs = true;
  bool qol_improvements = true;
  // Asset format (English or German DOS)
  platform::AssetFormat asset_format = platform::AssetFormat::ENGLISH;
  ScreenResolution resolution = ScreenResolution::Res_640x400;
  bool auto_save = true;
  bool rmb_drag_inverted = false;
  bool scroll_zoom_inverted = false;
  bool software_cursor = true;
  FpsOption render_fps = FpsOption::Fps60;
  bool vsync = true;
};
