/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/input/Key.h"

#include "engine/StringUtil.h"

#include <array>

namespace input {
namespace {

struct KeyAlias {
  std::string_view name;
  Key key;
};
// First entry for a key is the name key_to_string returns.
constexpr auto key_names = std::to_array<KeyAlias>({
    // letters
    {"A", Key::A},
    {"B", Key::B},
    {"C", Key::C},
    {"D", Key::D},
    {"E", Key::E},
    {"F", Key::F},
    {"G", Key::G},
    {"H", Key::H},
    {"I", Key::I},
    {"J", Key::J},
    {"K", Key::K},
    {"L", Key::L},
    {"M", Key::M},
    {"N", Key::N},
    {"O", Key::O},
    {"P", Key::P},
    {"Q", Key::Q},
    {"R", Key::R},
    {"S", Key::S},
    {"T", Key::T},
    {"U", Key::U},
    {"V", Key::V},
    {"W", Key::W},
    {"X", Key::X},
    {"Y", Key::Y},
    {"Z", Key::Z},

    // digits
    {"0", Key::Num0},
    {"1", Key::Num1},
    {"2", Key::Num2},
    {"3", Key::Num3},
    {"4", Key::Num4},
    {"5", Key::Num5},
    {"6", Key::Num6},
    {"7", Key::Num7},
    {"8", Key::Num8},
    {"9", Key::Num9},

    // punctuation (aliases included)
    {"Minus", Key::Minus},
    {"hyphen", Key::Minus},
    {"dash", Key::Minus},
    {"-", Key::Minus},
    {"Equals", Key::Equals},
    {"=", Key::Equals},
    {"LeftBracket", Key::LeftBracket},
    {"lbracket", Key::LeftBracket},
    {"[", Key::LeftBracket},
    {"RightBracket", Key::RightBracket},
    {"rbracket", Key::RightBracket},
    {"]", Key::RightBracket},
    {"Semicolon", Key::Semicolon},
    {";", Key::Semicolon},
    {"Quote", Key::Quote},
    {"apostrophe", Key::Quote},
    {"'", Key::Quote},
    {"Backslash", Key::Backslash},
    {"\\", Key::Backslash},
    {"Comma", Key::Comma},
    {",", Key::Comma},
    {"Period", Key::Period},
    {"dot", Key::Period},
    {".", Key::Period},
    {"Slash", Key::Slash},
    {"/", Key::Slash},
    {"Backquote", Key::Grave},
    {"grave", Key::Grave},
    {"`", Key::Grave},
    {"backtick", Key::Grave},

    // control
    {"Space", Key::Space},
    {"Tab", Key::Tab},
    {"Return", Key::Return},
    {"enter", Key::Return},
    {"Backspace", Key::Backspace},
    {"Escape", Key::Escape},
    {"esc", Key::Escape},

    // nav
    {"Left", Key::Left},
    {"Right", Key::Right},
    {"Up", Key::Up},
    {"Down", Key::Down},
    {"PageUp", Key::PageUp},
    {"PageDown", Key::PageDown},
    {"Home", Key::Home},
    {"End", Key::End},
    {"Insert", Key::Insert},
    {"Delete", Key::Delete},

    // function
    {"F1", Key::F1},
    {"F2", Key::F2},
    {"F3", Key::F3},
    {"F4", Key::F4},
    {"F5", Key::F5},
    {"F6", Key::F6},
    {"F7", Key::F7},
    {"F8", Key::F8},
    {"F9", Key::F9},
    {"F10", Key::F10},
    {"F11", Key::F11},
    {"F12", Key::F12},

    // modifiers as keys
    {"LShift", Key::LShift},
    {"RShift", Key::RShift},
    {"LCtrl", Key::LCtrl},
    {"RCtrl", Key::RCtrl},
    {"LAlt", Key::LAlt},
    {"RAlt", Key::RAlt},
    {"LGui", Key::LMeta},
    {"RGui", Key::RMeta},
});

struct ModAlias {
  std::string_view name;
  Mod mod;
};
constexpr auto mod_names = std::to_array<ModAlias>({{"shift", Mod::Shift},
                                                    {"ctrl", Mod::Ctrl},
                                                    {"control", Mod::Ctrl},
                                                    {"alt", Mod::Alt},
                                                    {"gui", Mod::Meta},
                                                    {"meta", Mod::Meta},
                                                    {"cmd", Mod::Meta},
                                                    {"super", Mod::Meta}});

inline std::optional<Mod> mod_from_name(std::string_view s) {
  Mod acc = Mod::None;
  size_t start = 0;
  // support "Ctrl+Shift" or comma-separated
  while (start <= s.size()) {
    size_t end = s.find_first_of("+,", start);
    auto token = s.substr(
        start, (end == std::string_view::npos ? s.size() : end) - start);
    if (!token.empty()) {
      bool hit = false;
      for (const auto& m : mod_names) {
        if (util::iequals(token, m.name)) {
          acc = acc | m.mod;
          hit = true;
          break;
        }
      }
      if (!hit) {
        return std::nullopt;
      }
    }
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return acc;
}

} // namespace

std::optional<Key> key_from_string(std::string_view sv) {
  for (const auto& a : key_names) {
    if (util::iequals(sv, a.name)) {
      return a.key;
    }
  }
  return std::nullopt;
}

std::string_view key_to_string(Key k) {
  for (const auto& a : key_names) {
    if (a.key == k) {
      return a.name;
    }
  }
  return "unknown";
}

std::optional<std::pair<Mod, Key>> chord_split(std::string_view s) {
  // find last '+' and split "mods + key"
  const auto pos = s.rfind('+');
  std::string_view key_part;
  std::string_view mod_part;
  if (pos == std::string_view::npos) {
    key_part = s;
  }
  else {
    mod_part = s.substr(0, pos);
    key_part = s.substr(pos + 1);
  }
  auto k = key_from_string(key_part);
  if (!k) {
    return std::nullopt;
  }
  Mod m = Mod::None;
  if (!mod_part.empty()) {
    auto mm = mod_from_name(mod_part);
    if (!mm) {
      return std::nullopt;
    }
    m = *mm;
  }
  return std::make_pair(m, *k);
}

std::optional<Binding> parse_chord(std::string_view s) {
  if (auto mk = chord_split(s)) {
    return Binding{.mods = mk->first, .key = mk->second};
  }
  return std::nullopt;
}

std::string chord_to_string(const Binding& b) {
  std::string out;
  if (has(b.mods, Mod::Ctrl)) {
    out += "Ctrl+";
  }
  if (has(b.mods, Mod::Shift)) {
    out += "Shift+";
  }
  if (has(b.mods, Mod::Alt)) {
    out += "Alt+";
  }
  if (has(b.mods, Mod::Meta)) {
    out += "Meta+";
  }
  out += key_to_string(b.key);
  return out;
}
std::optional<Mod> mods_from_string(std::string_view s) {
  return mod_from_name(s);
}

} // namespace input
