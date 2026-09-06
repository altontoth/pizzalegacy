/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>

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

// Market share domination needed to win. The English release checks Paris
// alone, apparently meaning any one city; the German one checks every city.
enum class WinCondition { Paris, AnyCity, AllCities };

inline std::string_view to_config_value(WinCondition condition) {
  switch (condition) {
  case WinCondition::Paris:
    return "paris";
  case WinCondition::AnyCity:
    return "any_city";
  case WinCondition::AllCities:
    return "all_cities";
  }
  return "any_city";
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

inline std::optional<WinCondition>
win_condition_from_config_value(std::string_view value) {
  if (value == "paris") {
    return WinCondition::Paris;
  }
  if (value == "any_city") {
    return WinCondition::AnyCity;
  }
  if (value == "all_cities") {
    return WinCondition::AllCities;
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
  // Start the window at twice the 640x400 surface.
  bool double_size = true;
  bool fullscreen = false;
  AutosaveFrequency autosave_frequency = AutosaveFrequency::Daily;
  // Number of rotating autosave slot files.
  int autosave_count = 3;
  bool rmb_drag_inverted = false;
  bool scroll_zoom_inverted = false;
  bool software_cursor = true;
  FpsOption render_fps = FpsOption::Fps60;
  bool vsync = true;

  bool load_list_sort_by_date = true;
  bool load_list_show_autosaves = false;

  // Config file only, with no entry in the options screen.
  // The mafia shakes down a random restaurant instead of always the first.
  bool mafia_shakedowns_target_all_restaurants = false;
  WinCondition win_condition = WinCondition::AnyCity;
  // How long a held key waits before it starts repeating, and the gap
  // between repeats.
  int key_repeat_delay_ms = 500;
  int key_repeat_interval_ms = 60;
};
