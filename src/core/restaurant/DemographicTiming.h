/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include "core/Demographics.h"

// Minutes a customer of each demographic spends eating.
constexpr std::array<int, static_cast<size_t>(ConsumerType::Count)>
    DEMOGRAPHIC_EATING_TIME = {
        12, // Children
        10, // Young People
        8,  // Blue Collar
        10, // White Collar
        7,  // Business People
        15, // Senior Citizens
};

inline int demographic_eating_time(ConsumerType demographic) {
  return DEMOGRAPHIC_EATING_TIME[static_cast<size_t>(demographic)];
}
