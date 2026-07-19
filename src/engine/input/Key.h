/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace input {

// Physical keys
enum class Key {
  Unknown = 0,

  A,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,
  Num0,
  Num1,
  Num2,
  Num3,
  Num4,
  Num5,
  Num6,
  Num7,
  Num8,
  Num9,

  Minus,
  Equals,
  LeftBracket,
  RightBracket,
  Semicolon,
  Quote,
  Backslash,
  Comma,
  Period,
  Slash,
  Grave,

  Space,
  Tab,
  Return,
  Backspace,
  Escape,

  Left,
  Right,
  Up,
  Down,
  PageUp,
  PageDown,
  Home,
  End,
  Insert,
  Delete,

  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,

  LShift,
  RShift,
  LCtrl,
  RCtrl,
  LAlt,
  RAlt,
  LMeta,
  RMeta,
};

enum class Mod : uint8_t {
  None = 0,
  Shift = 1 << 0,
  Ctrl = 1 << 1,
  Alt = 1 << 2,
  Meta = 1 << 3
};
inline Mod operator|(Mod a, Mod b) {
  return static_cast<Mod>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline bool has(Mod m, Mod bit) {
  return (static_cast<uint8_t>(m) & static_cast<uint8_t>(bit)) != 0;
}

struct Binding {
  Mod mods{Mod::None};
  Key key{Key::Unknown};

  bool operator==(const Binding&) const = default;
};

/// Parse "Ctrl+Shift+B", "Backquote", "PageDown", case-insensitive, with
/// aliases. Returns nullopt on unknown input.
std::optional<Binding> parse_chord(std::string_view s);

/// Serialize with canonical names and canonical mod order: "Ctrl+Shift+B".
std::string chord_to_string(const Binding&);

/// Parse a single key name like "A", "LeftBracket", "Minus", "Backquote".
std::optional<Key> key_from_string(std::string_view);

/// Canonical key name ("A", "Minus", "Backquote", "PageDown"...)
std::string_view key_to_string(Key);

/// Parse a list of modifier names or a combined "+" chord into a Mod bitmask.
std::optional<Mod> mods_from_string(std::string_view);

/// Split a "+" chord string into (mods, key); identical to parse_chord but
/// returns Mod separately.
std::optional<std::pair<Mod, Key>> chord_split(std::string_view);

} // namespace input
