/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include "core/Demographics.h"

// Time added to service pipeline per customer demographic (in minutes)
// This represents eating time, payment time, and other customer-specific delays
constexpr std::array<int, static_cast<size_t>(ConsumerType::Count)>
    DEMOGRAPHIC_SERVICE_TIME = {
        12, // Children
        10, // Young People
        8,  // Blue Collar
        10, // White Collar
        7,  // Business People
        15, // Senior Citizens
};

// Helper function to get demographic service time
inline int get_demographic_service_time(ConsumerType demographic) {
  return DEMOGRAPHIC_SERVICE_TIME[static_cast<size_t>(demographic)];
}
