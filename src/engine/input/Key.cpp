/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/input/Key.h"

#include <algorithm>

namespace input {
namespace {

inline std::string lower(std::string_view sv) {
  std::string s(sv);
  std::ranges::transform(s, s.begin(),
                         [](unsigned char c) { return std::tolower(c); });
  return s;
}

struct KeyAlias {
  std::string_view name;
  Key key;
};
constexpr KeyAlias key_names[] = {
    // letters
    {"a", Key::A},
    {"b", Key::B},
    {"c", Key::C},
    {"d", Key::D},
    {"e", Key::E},
    {"f", Key::F},
    {"g", Key::G},
    {"h", Key::H},
    {"i", Key::I},
    {"j", Key::J},
    {"k", Key::K},
    {"l", Key::L},
    {"m", Key::M},
    {"n", Key::N},
    {"o", Key::O},
    {"p", Key::P},
    {"q", Key::Q},
    {"r", Key::R},
    {"s", Key::S},
    {"t", Key::T},
    {"u", Key::U},
    {"v", Key::V},
    {"w", Key::W},
    {"x", Key::X},
    {"y", Key::Y},
    {"z", Key::Z},

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
    {"minus", Key::Minus},
    {"hyphen", Key::Minus},
    {"dash", Key::Minus},
    {"-", Key::Minus},
    {"equals", Key::Equals},
    {"=", Key::Equals},
    {"leftbracket", Key::LeftBracket},
    {"lbracket", Key::LeftBracket},
    {"[", Key::LeftBracket},
    {"rightbracket", Key::RightBracket},
    {"rbracket", Key::RightBracket},
    {"]", Key::RightBracket},
    {"semicolon", Key::Semicolon},
    {";", Key::Semicolon},
    {"quote", Key::Quote},
    {"apostrophe", Key::Quote},
    {"'", Key::Quote},
    {"backslash", Key::Backslash},
    {"\\", Key::Backslash},
    {"comma", Key::Comma},
    {",", Key::Comma},
    {"period", Key::Period},
    {"dot", Key::Period},
    {".", Key::Period},
    {"slash", Key::Slash},
    {"/", Key::Slash},
    {"grave", Key::Grave},
    {"backquote", Key::Grave},
    {"`", Key::Grave},
    {"backtick", Key::Grave},

    // control
    {"space", Key::Space},
    {"tab", Key::Tab},
    {"return", Key::Return},
    {"enter", Key::Return},
    {"backspace", Key::Backspace},
    {"escape", Key::Escape},
    {"esc", Key::Escape},

    // nav
    {"left", Key::Left},
    {"right", Key::Right},
    {"up", Key::Up},
    {"down", Key::Down},
    {"pageup", Key::PageUp},
    {"pagedown", Key::PageDown},
    {"home", Key::Home},
    {"end", Key::End},
    {"insert", Key::Insert},
    {"delete", Key::Delete},

    // function
    {"f1", Key::F1},
    {"f2", Key::F2},
    {"f3", Key::F3},
    {"f4", Key::F4},
    {"f5", Key::F5},
    {"f6", Key::F6},
    {"f7", Key::F7},
    {"f8", Key::F8},
    {"f9", Key::F9},
    {"f10", Key::F10},
    {"f11", Key::F11},
    {"f12", Key::F12},

    // modifiers as keys
    {"lshift", Key::LShift},
    {"rshift", Key::RShift},
    {"lctrl", Key::LCtrl},
    {"rctrl", Key::RCtrl},
    {"lalt", Key::LAlt},
    {"ralt", Key::RAlt},
    {"lgui", Key::LMeta},
    {"rgui", Key::RMeta},
};

struct ModAlias {
  std::string_view name;
  Mod mod;
};
constexpr ModAlias mod_names[] = {{"shift", Mod::Shift},  {"ctrl", Mod::Ctrl},
                                  {"control", Mod::Ctrl}, {"alt", Mod::Alt},
                                  {"gui", Mod::Meta},     {"meta", Mod::Meta},
                                  {"cmd", Mod::Meta},     {"super", Mod::Meta}};

inline std::optional<Mod> mod_from_name(std::string_view sv) {
  const auto s = lower(sv);
  Mod acc = Mod::None;
  size_t start = 0;
  // support "Ctrl+Shift" or comma-separated
  while (start <= s.size()) {
    size_t end = s.find_first_of("+,", start);
    auto token =
        s.substr(start, (end == std::string::npos ? s.size() : end) - start);
    if (!token.empty()) {
      bool hit = false;
      for (auto& m : mod_names) {
        if (token == m.name) {
          acc = acc | m.mod;
          hit = true;
          break;
        }
      }
      if (!hit) {
        return std::nullopt;
      }
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return acc;
}

} // namespace

std::optional<Key> key_from_string(std::string_view sv) {
  const auto s = lower(sv);
  for (const auto& a : key_names) {
    if (s == a.name) {
      return a.key;
    }
  }
  return std::nullopt;
}

std::string_view key_to_string(Key k) {
  for (const auto& a : key_names) {
    if (a.key == k) {
      return a.name; // returns lower-case
    }
  }
  return "unknown";
}

std::optional<std::pair<Mod, Key>> chord_split(std::string_view s) {
  // find last '+' and split "mods + key"
  auto ls = lower(s);
  auto pos = ls.rfind('+');
  std::string_view key_part;
  std::string_view mod_part;
  if (pos == std::string::npos) {
    key_part = s;
  }
  else {
    mod_part = std::string_view(s.data(), pos);
    key_part = std::string_view(s.data() + pos + 1, s.size() - pos - 1);
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
