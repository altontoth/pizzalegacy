/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/Math.h"

#include <cstdlib>

namespace math {

uint16_t two_bytes_to_uint16(unsigned char a, unsigned char b, Endian e) {
  uint8_t byte1 = a;
  uint8_t byte2 = b;
  if (e == Endian::Big) {
    return (byte1 << 8) | byte2;
  }
  return (byte2 << 8) | byte1;
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
