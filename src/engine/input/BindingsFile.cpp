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
#include "engine/StringUtil.h"
#include "engine/input/Action.h"
#include "engine/input/Gamepad.h"
#include "engine/input/InputManager.h"
#include "engine/input/Key.h"
#include "magic_enum/magic_enum.hpp"

namespace input {
namespace { // anonymous

std::string lower(std::string_view sv) {
  std::string s(sv);
  std::ranges::transform(s, s.begin(),
                         [](unsigned char c) { return std::tolower(c); });
  return s;
}

constexpr std::string_view pad_prefix = "pad.";
// Gamepad prefix as written back out to the bindings template.
constexpr std::string_view pad_prefix_out = "Pad.";

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
    const std::string_view stripped = util::trim(line);
    if (stripped.empty() || stripped.starts_with('#')) {
      continue;
    }

    const auto eq = stripped.find('=');
    if (eq == std::string_view::npos) {
      Logger::warn("{}:{}: expected 'Action=Binding[,Binding...]'", path,
                   line_no);
      continue;
    }

    const std::string_view action_name = util::trim(stripped.substr(0, eq));
    const auto action = magic_enum::enum_cast<Action>(
        action_name, magic_enum::case_insensitive);
    if (!action) {
      Logger::warn("{}:{}: unknown action '{}'", path, line_no, action_name);
      continue;
    }

    if (in.is_fixed(*action)) {
      Logger::warn("{}:{}: '{}' cannot be reassigned", path, line_no,
                   action_name);
      continue;
    }

    in.remove_bindings(*action);

    std::string_view rest = util::trim(stripped.substr(eq + 1));
    while (!rest.empty()) {
      const auto comma = rest.find(',');
      const std::string_view token = util::trim(rest.substr(0, comma));
      rest = (comma == std::string_view::npos)
                 ? std::string_view{}
                 : util::trim(rest.substr(comma + 1));
      if (!token.empty()) {
        bind_token(in, *action, token, path, line_no);
      }
    }
  }
}

namespace { // anonymous

std::string bindings_for(const InputManager& in, Action a) {
  std::string joined;
  auto append = [&joined](std::string_view token) {
    if (!joined.empty()) {
      joined += ',';
    }
    joined += token;
  };

  for (const Binding& chord : in.button_bindings_for(a)) {
    append(chord_to_string(chord));
  }
  for (const auto& [button, action] : in.gamepad_button_bindings()) {
    if (action == a) {
      append(std::string(pad_prefix_out) +
             std::string(gamepad_button_to_string(button)));
    }
  }
  return joined;
}

} // namespace

void write_bindings_template(const InputManager& in, const std::string& path) {
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
# Everything below is this build's defaults. An action listed with no value
# has no default binding; give it a chord to enable it.
)";

  const auto& actions = in.documented_actions();
  const auto& sections = in.doc_sections();
  std::size_t next_section = 0;

  for (std::size_t i = 0; i < actions.size(); ++i) {
    while (next_section < sections.size() &&
           sections[next_section].first_action == i) {
      file << "#\n# " << sections[next_section].label << ":\n";
      ++next_section;
    }
    file << '#' << magic_enum::enum_name(actions[i]) << '='
         << bindings_for(in, actions[i]) << '\n';
  }
}

} // namespace input
