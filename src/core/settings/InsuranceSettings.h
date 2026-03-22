/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

struct InsuranceSettings {
  uint8_t premium_factor;        // Affects insurance premium pricing
  uint8_t willingness_threshold; // Threshold for accepting claims
  uint8_t rejection_percentage;  // Chance of rejecting a claim (%)
  uint8_t padding;               // Always 0x00

  InsuranceSettings(uint8_t premium, uint8_t willingness, uint8_t rejection,
                    uint8_t pad = 0)
      : premium_factor(premium), willingness_threshold(willingness),
        rejection_percentage(rejection), padding(pad) {}
};
