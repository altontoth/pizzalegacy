/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/state/GameRules.h"
#include "engine/DisplayConstants.h"

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

enum class AutosaveFrequency : int { Off, Daily, Weekly, Monthly };

inline std::string to_string(AutosaveFrequency freq) {
  switch (freq) {
  case AutosaveFrequency::Off:
    return "Off";
  case AutosaveFrequency::Daily:
    return "Daily";
  case AutosaveFrequency::Weekly:
    return "Weekly";
  case AutosaveFrequency::Monthly:
    return "Monthly";
  }
  return "??";
}

inline std::string_view to_config_value(FpsOption fps) {
  switch (fps) {
  case FpsOption::Fps30:
    return "30";
  case FpsOption::Fps60:
    return "60";
  case FpsOption::Fps120:
    return "120";
  }
  return "60";
}

inline std::optional<FpsOption>
fps_option_from_config_value(std::string_view value) {
  if (value == "30") {
    return FpsOption::Fps30;
  }
  if (value == "60") {
    return FpsOption::Fps60;
  }
  if (value == "120") {
    return FpsOption::Fps120;
  }
  return std::nullopt;
}

inline std::string_view to_config_value(AutosaveFrequency frequency) {
  switch (frequency) {
  case AutosaveFrequency::Off:
    return "off";
  case AutosaveFrequency::Daily:
    return "daily";
  case AutosaveFrequency::Weekly:
    return "weekly";
  case AutosaveFrequency::Monthly:
    return "monthly";
  }
  return "daily";
}

inline std::optional<AutosaveFrequency>
autosave_frequency_from_config_value(std::string_view value) {
  if (value == "off") {
    return AutosaveFrequency::Off;
  }
  if (value == "daily") {
    return AutosaveFrequency::Daily;
  }
  if (value == "weekly") {
    return AutosaveFrequency::Weekly;
  }
  if (value == "monthly") {
    return AutosaveFrequency::Monthly;
  }
  return std::nullopt;
}

struct GameOptions {
  static constexpr int AUTOSAVE_COUNT_MIN = 1;
  static constexpr int AUTOSAVE_COUNT_MAX = 9;
  static constexpr int KEY_REPEAT_DELAY_MIN = 50;
  static constexpr int KEY_REPEAT_DELAY_MAX = 2000;
  static constexpr int KEY_REPEAT_INTERVAL_MIN = 10;
  static constexpr int KEY_REPEAT_INTERVAL_MAX = 1000;
  static constexpr int TOOLTIP_DELAY_MIN = 0;
  static constexpr int TOOLTIP_DELAY_MAX = 5000;
  static constexpr int ATTACK_SCENE_HZ_MIN = 15;
  static constexpr int ATTACK_SCENE_HZ_MAX = 100;

  bool map_fx = true;
  bool restaurant_fx = true;
  bool music = true;
  float sound_volume = 0.9f;
  float music_volume = 1.0f;
  // Show the day's turnover report when the turn ends.
  bool automatic_report = true;
  // Mouse and UI modernizations:
  bool qol_improvements = true;
  // Use 640x480 (4:3) CRT aspect ratio instead of 640x400
  bool crt_aspect_ratio = false;
  // Start the window at twice the 640x400 surface.
  bool double_size = true;
  bool fullscreen = false;
  AutosaveFrequency autosave_frequency = AutosaveFrequency::Daily;
  // Number of rotating autosave slot files.
  int autosave_count = 3;
  bool rmb_drag_inverted = false;
  bool scroll_wheel_inverted = false;
  bool software_cursor = true;
  FpsOption render_fps = FpsOption::Fps60;
  bool vsync = true;

  bool load_list_sort_by_date = true;
  bool load_list_show_autosaves = false;

  // The rules a new game starts with.
  GameRules rules;
  // How long a held key waits before it starts repeating, and the gap
  // between repeats.
  int key_repeat_delay_ms = 500;
  int key_repeat_interval_ms = 60;
  int tooltip_delay_ms = 300;
  bool show_player_color_in_dropdown = true;
  // Attack scene updates per second. The original updates once for every frame
  // it draws, which is 70 on a fast 486 and 35 or less on a slower machine.
  int attack_scene_hz = 35;

  // Not saved, so it lasts for this run only.
  // Show the advanced rows on the options screens.
  bool show_advanced_options = false;
};
