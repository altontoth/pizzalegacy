/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <span>

enum class Endian { Little, Big };

namespace math {

uint16_t two_bytes_to_uint16(uint8_t a, uint8_t b, Endian e = Endian::Little);

// Fancy schmancy constexpr template for optimal performance :D
// This thing just rounds up to the specified power of ten,
// e.g. with <3> to the nearest 1000.
template <int Exp> constexpr int64_t round_up_to_next_power10(int64_t v) {
  constexpr int64_t base = [] {
    int64_t b = 1;
    for (int i = 0; i < Exp; ++i) {
      b *= 10;
    }
    return b;
  }();
  return ((v + base - 1) / base) * base;
}

uint32_t sum_bytes(std::span<const std::byte> data);
} // namespace math
