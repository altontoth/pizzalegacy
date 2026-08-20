/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <span>

namespace math {

int64_t round_up_power10(int64_t v);

// Fancy schmancy constexpr template for optimal performance :D
// Advances to the next multiple of the specified power of ten, e.g. with <3>
// to the next 1000. A value already on a multiple still moves up a whole step.
template <int Exp> constexpr int64_t round_up_to_next_power10(int64_t v) {
  constexpr int64_t base = [] {
    int64_t b = 1;
    for (int i = 0; i < Exp; ++i) {
      b *= 10;
    }
    return b;
  }();
  return ((v / base) * base) + base;
}

uint32_t sum_bytes(std::span<const std::byte> data);
} // namespace math
