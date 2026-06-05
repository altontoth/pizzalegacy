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
constexpr ButtonAlias k_button_names[] = {
    {"a", GamepadButton::A},
    {"b", GamepadButton::B},
    {"x", GamepadButton::X},
    {"y", GamepadButton::Y},
    {"back", GamepadButton::Back},
    {"guide", GamepadButton::Guide},
    {"start", GamepadButton::Start},
    {"leftstick", GamepadButton::LeftStick},
    {"rightstick", GamepadButton::RightStick},
    {"leftshoulder", GamepadButton::LeftShoulder},
    {"rightshoulder", GamepadButton::RightShoulder},
    {"dpadup", GamepadButton::DpadUp},
    {"dpaddown", GamepadButton::DpadDown},
    {"dpadleft", GamepadButton::DpadLeft},
    {"dpadright", GamepadButton::DpadRight},
};

struct AxisAlias {
  std::string_view name;
  GamepadAxis axis;
};
constexpr AxisAlias k_axis_names[] = {
    {"leftx", GamepadAxis::LeftX},
    {"lefty", GamepadAxis::LeftY},
    {"rightx", GamepadAxis::RightX},
    {"righty", GamepadAxis::RightY},
    {"triggerleft", GamepadAxis::TriggerLeft},
    {"triggerright", GamepadAxis::TriggerRight},
};

} // namespace

std::string_view gamepad_button_to_string(GamepadButton b) {
  for (const auto& a : k_button_names) {
    if (a.button == b) {
      return a.name;
    }
  }
  return "unknown";
}

std::optional<GamepadButton> gamepad_button_from_string(std::string_view sv) {
  const auto s = lower(sv);
  for (const auto& a : k_button_names) {
    if (s == a.name) {
      return a.button;
    }
  }
  return std::nullopt;
}

std::string_view gamepad_axis_to_string(GamepadAxis ax) {
  for (const auto& a : k_axis_names) {
    if (a.axis == ax) {
      return a.name;
    }
  }
  return "unknown";
}

std::optional<GamepadAxis> gamepad_axis_from_string(std::string_view sv) {
  const auto s = lower(sv);
  for (const auto& a : k_axis_names) {
    if (s == a.name) {
      return a.axis;
    }
  }
  return std::nullopt;
}

} // namespace input
