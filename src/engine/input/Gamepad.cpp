/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/input/Gamepad.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace input {
namespace { // anonymous

inline std::string lower(std::string_view sv) {
  std::string s(sv);
  std::ranges::transform(s, s.begin(),
                         [](unsigned char c) { return std::tolower(c); });
  return s;
}

struct ButtonAlias {
  std::string_view name;
  GamepadButton button;
};
constexpr ButtonAlias button_names[] = {
    {"A", GamepadButton::A},
    {"B", GamepadButton::B},
    {"X", GamepadButton::X},
    {"Y", GamepadButton::Y},
    {"Back", GamepadButton::Back},
    {"Guide", GamepadButton::Guide},
    {"Start", GamepadButton::Start},
    {"LeftStick", GamepadButton::LeftStick},
    {"RightStick", GamepadButton::RightStick},
    {"LeftShoulder", GamepadButton::LeftShoulder},
    {"RightShoulder", GamepadButton::RightShoulder},
    {"DpadUp", GamepadButton::DpadUp},
    {"DpadDown", GamepadButton::DpadDown},
    {"DpadLeft", GamepadButton::DpadLeft},
    {"DpadRight", GamepadButton::DpadRight},
    {"LeftStickUp", GamepadButton::LeftStickUp},
    {"LeftStickDown", GamepadButton::LeftStickDown},
    {"LeftStickLeft", GamepadButton::LeftStickLeft},
    {"LeftStickRight", GamepadButton::LeftStickRight},
    {"RightStickUp", GamepadButton::RightStickUp},
    {"RightStickDown", GamepadButton::RightStickDown},
    {"RightStickLeft", GamepadButton::RightStickLeft},
    {"RightStickRight", GamepadButton::RightStickRight},
};

struct AxisAlias {
  std::string_view name;
  GamepadAxis axis;
};
constexpr AxisAlias axis_names[] = {
    {"LeftX", GamepadAxis::LeftX},
    {"LeftY", GamepadAxis::LeftY},
    {"RightX", GamepadAxis::RightX},
    {"RightY", GamepadAxis::RightY},
    {"TriggerLeft", GamepadAxis::TriggerLeft},
    {"TriggerRight", GamepadAxis::TriggerRight},
};

} // namespace

std::string_view gamepad_button_to_string(GamepadButton b) {
  for (const auto& a : button_names) {
    if (a.button == b) {
      return a.name;
    }
  }
  return "unknown";
}

std::optional<GamepadButton> gamepad_button_from_string(std::string_view sv) {
  const auto s = lower(sv);
  for (const auto& a : button_names) {
    if (s == lower(a.name)) {
      return a.button;
    }
  }
  return std::nullopt;
}

std::string_view gamepad_axis_to_string(GamepadAxis ax) {
  for (const auto& a : axis_names) {
    if (a.axis == ax) {
      return a.name;
    }
  }
  return "unknown";
}

std::optional<GamepadAxis> gamepad_axis_from_string(std::string_view sv) {
  const auto s = lower(sv);
  for (const auto& a : axis_names) {
    if (s == lower(a.name)) {
      return a.axis;
    }
  }
  return std::nullopt;
}

} // namespace input
