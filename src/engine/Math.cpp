/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/Math.h"

#include <cstdlib>

namespace math {

int64_t round_up_power10(int64_t v) {
  int64_t p = 1;
  while (p < v) {
    p *= 10;
  }
  return p;
}

uint32_t sum_bytes(std::span<const std::byte> data) {
  uint32_t s = 0;
  for (auto b : data) {
    s += std::to_integer<uint8_t>(b);
  }
  return s; // modulo 2^32 because we're using an uint32_t here (as does the
            // original)
}

} // namespace math
