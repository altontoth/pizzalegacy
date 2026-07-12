/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/input/BindingsFile.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

#include "engine/Logger.h"
#include "engine/input/Action.h"
#include "engine/input/Gamepad.h"
#include "engine/input/InputManager.h"
#include "engine/input/Key.h"
#include "magic_enum/magic_enum.hpp"

namespace input {
namespace { // anonymous

std::string_view trim(std::string_view sv) {
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
    sv.remove_prefix(1);
  }
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
    sv.remove_suffix(1);
  }
  return sv;
}

std::string lower(std::string_view sv) {
  std::string s(sv);
  std::ranges::transform(s, s.begin(),
                         [](unsigned char c) { return std::tolower(c); });
  return s;
}

constexpr std::string_view pad_prefix = "pad.";

void bind_token(InputManager& in, Action action, std::string_view token,
                const std::string& path, int line_no) {
  const std::string token_lower = lower(token);

  if (token_lower.starts_with(pad_prefix)) {
    const std::string_view name =
        std::string_view(token_lower).substr(pad_prefix.size());
    if (auto button = gamepad_button_from_string(name)) {
      in.bind_gamepad_button(*button, action);
    }
    else {
      Logger::warn("{}:{}: unknown gamepad button '{}'", path, line_no, token);
    }
    return;
  }

  if (auto chord = parse_chord(token)) {
    in.bind_button(*chord, action);
  }
  else {
    Logger::warn("{}:{}: unknown key chord '{}'", path, line_no, token);
  }
}

} // namespace

void apply_bindings_file(InputManager& in, const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    return;
  }

  std::string line;
  int line_no = 0;
  while (std::getline(file, line)) {
    ++line_no;
    const std::string_view stripped = trim(line);
    if (stripped.empty() || stripped.starts_with('#')) {
      continue;
    }

    const auto eq = stripped.find('=');
    if (eq == std::string_view::npos) {
      Logger::warn("{}:{}: expected 'Action=Binding[,Binding...]'", path,
                   line_no);
      continue;
    }

    const std::string_view action_name = trim(stripped.substr(0, eq));
    const auto action = magic_enum::enum_cast<Action>(
        action_name, magic_enum::case_insensitive);
    if (!action) {
      Logger::warn("{}:{}: unknown action '{}'", path, line_no, action_name);
      continue;
    }

    in.remove_bindings(*action);

    std::string_view rest = trim(stripped.substr(eq + 1));
    while (!rest.empty()) {
      const auto comma = rest.find(',');
      const std::string_view token = trim(rest.substr(0, comma));
      rest = (comma == std::string_view::npos) ? std::string_view{}
                                               : trim(rest.substr(comma + 1));
      if (!token.empty()) {
        bind_token(in, *action, token, path, line_no);
      }
    }
  }
}

void write_bindings_template(const std::string& path) {
  if (std::filesystem::exists(path)) {
    return;
  }

  std::ofstream file(path);
  if (!file) {
    Logger::warn("Could not write bindings template to {}", path);
    return;
  }

  file << R"(# Pizza Legacy key bindings
#
# Each uncommented line binds one action, replacing its defaults:
#
#   Action=Binding[,Binding...]
#
# A binding is a keyboard chord ("F", "Ctrl+F", "Shift+Tab") or a gamepad
# input prefixed with "Pad." ("Pad.A", "Pad.DpadLeft", "Pad.Start").
# The analog sticks act as four directional buttons each:
# Pad.LeftStickUp/Down/Left/Right and Pad.RightStickUp/Down/Left/Right.
# Pad.LeftStick and Pad.RightStick are the stick-click buttons.
# An empty value ("TogglePause=") unbinds the action entirely.
#
# Names are case-insensitive. Lines starting with # are comments.
# Delete this file to restore it with all defaults.
#
# The defaults:
#
#ToggleFullscreen=Ctrl+F
#TakeScreenshot=Ctrl+S
#TogglePause=Ctrl+P
#ToggleConsole=Backtick
#Exit=Escape
#PageUp=PageUp
#PageDown=PageDown
#
# Player movement and menu navigation:
#MoveLeft=Left,Pad.DpadLeft
#MoveRight=Right,Pad.DpadRight
#MoveUp=Up,Pad.DpadUp
#MoveDown=Down,Pad.DpadDown
#Fire=Space
#
# Map scrolling:
#ScrollLeft=Left
#ScrollRight=Right
#ScrollUp=Up
#ScrollDown=Down
#
# Second player (defender in the showdown):
#P2_MoveLeft=A,Pad.LeftStickLeft
#P2_MoveRight=D,Pad.LeftStickRight
#P2_MoveUp=W,Pad.LeftStickUp
#P2_MoveDown=S,Pad.LeftStickDown
#P2_Fire=LCtrl,Pad.A
#
#NextRestaurant=Tab
#PreviousRestaurant=Shift+Tab
#ToggleRestaurantView=Space
#
#ConfirmYes=Y,Enter
#ConfirmNo=N
#NextMenuItem=Tab
#PrevMenuItem=Shift+Tab
)";
}

} // namespace input
