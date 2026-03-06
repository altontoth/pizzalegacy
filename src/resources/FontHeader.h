/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstdint>
#include <ostream>

#pragma pack(1)
struct FontHeader {
  uint16_t character_height;
  uint8_t padding;
  uint8_t fixed_width_font_flag; // 0x02 if fixed with, 0x62 if not.
  uint16_t character_width;
  uint16_t baseline_height;
  uint8_t first_character;
  uint8_t last_character;
  uint8_t start_at_bit;
  uint8_t u8;
  uint8_t u9;
  uint8_t u10;
  uint16_t number_of_bytes_per_row;
  uint16_t some_constant;
  uint8_t u13;
  uint8_t u14;
  uint8_t u15;
  uint8_t u16;
  uint16_t another_constant;
};

inline std::ostream& operator<<(std::ostream& os, const FontHeader& h) {
  os << "FontHeader {\n";
  os << "  character_height: " << +h.character_height << "\n";
  os << "  padding: " << +h.padding << "\n";
  os << "  fixedWidth: " << +h.fixed_width_font_flag << "\n";
  os << "  character_width: " << h.character_width << "\n";
  os << "  baseline: " << +h.baseline_height << "\n";
  os << "  firstChar: " << +h.first_character << "\n";
  os << "  lastChar: " << +h.last_character << "\n";
  os << "  start_at_bit: " << +h.start_at_bit << "\n";
  os << "  u8: " << +h.u8 << "\n";
  os << "  u9: " << +h.u9 << "\n";
  os << "  u10: " << +h.u10 << "\n";
  os << "  number_of_bytes_per_row: " << h.number_of_bytes_per_row << "\n";
  os << "  constant: " << +h.some_constant << "\n";
  os << "  u13: " << +h.u13 << "\n";
  os << "  u14: " << +h.u14 << "\n";
  os << "  u15: " << +h.u15 << "\n";
  os << "  u16: " << +h.u16 << "\n";
  os << "  constant: " << +h.another_constant << "\n";
  os << "}";
  return os;
}

struct CharacterPosition {
  uint16_t start_pos{0};
  uint16_t width{0};
};
#pragma pack()
