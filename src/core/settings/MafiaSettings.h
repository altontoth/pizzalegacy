/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

struct MafiaSettings {
  uint8_t crime_danger_level; // Overall danger/crime level for this mafia actor
  uint8_t unknown_1;          // Unused in original
  uint8_t unknown_2;          // Unused in original
  uint8_t unknown_3;          // Unused in original

  MafiaSettings(uint8_t danger, uint8_t unk1 = 0, uint8_t unk2 = 0,
                uint8_t unk3 = 0)
      : crime_danger_level(danger), unknown_1(unk1), unknown_2(unk2),
        unknown_3(unk3) {}
};
