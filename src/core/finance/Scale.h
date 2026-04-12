/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>

#include "engine/Math.h"

template <typename It> int compute_scale(It begin, It end, int max_height) {
  int64_t max_value = 0;

  for (auto it = begin; it != end; ++it) {
    max_value = std::max<int64_t>(max_value, std::abs(*it));
  }

  if (max_value == 0) {
    return 100;
  }

  int64_t raw_scale = (max_value + max_height - 1) / max_height;

  return static_cast<int>(math::round_up_power10(raw_scale));
}
