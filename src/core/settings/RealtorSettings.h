/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

struct RealtorSettings {
  uint8_t unknown_1;      // Mystery byte 1, seems unused but does have a value
  uint8_t unknown_2;      // Mystery byte 2, seems unused but does have a value
  uint8_t rip_off_chance; // Chance the realtor will try to rip you off by
                          // doubling the price of a property they're selling.
  uint8_t padding;        // Always 0x00

  // Constructor for easy initialization
  RealtorSettings(uint8_t u_1, uint8_t u_2, uint8_t ro_chance = 0,
                  uint8_t pad = 0)
      : unknown_1(u_1), unknown_2(u_2), rip_off_chance(ro_chance),
        padding(pad) {}
};
