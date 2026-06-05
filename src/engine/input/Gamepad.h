/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <optional>
#include <string_view>

namespace input {

enum class GamepadButton {
  Unknown = 0,
  A,
  B,
  X,
  Y,
  Back,
  Guide,
  Start,
  LeftStick,
  RightStick,
  LeftShoulder,
  RightShoulder,
  DpadUp,
  DpadDown,
  DpadLeft,
  DpadRight,
};

enum class GamepadAxis {
  Unknown = 0,
  LeftX,
  LeftY,
  RightX,
  RightY,
  TriggerLeft,
  TriggerRight,
};

std::string_view gamepad_button_to_string(GamepadButton b);
std::optional<GamepadButton> gamepad_button_from_string(std::string_view s);
std::string_view gamepad_axis_to_string(GamepadAxis a);
std::optional<GamepadAxis> gamepad_axis_from_string(std::string_view s);

} // namespace input
