/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

struct BankSettings {
  uint8_t interest_rate_modifier; // Added to base interest rate (%)
  uint8_t loan_amount_multiplier; // Multiplier for loan amount calculation
  uint8_t unused_byte;            // Mystery third byte, no usage found yet
  uint8_t padding;                // Always 0x00

  // Constructor for easy initialization
  BankSettings(uint8_t interest_mod, uint8_t loan_mult, uint8_t unused = 0,
               uint8_t pad = 0)
      : interest_rate_modifier(interest_mod), loan_amount_multiplier(loan_mult),
        unused_byte(unused), padding(pad) {}
};
