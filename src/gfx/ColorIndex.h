/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <optional>

namespace gfx {

// Platform-aware color index that holds separate DOS and Amiga palette indices.
// When only one value is provided, it's used for both platforms.
// When two values are provided, the appropriate one is selected based on the
// active asset format.
struct ColorIndex {
  uint8_t dos;
  std::optional<uint8_t> amiga;

  // Single value constructor - uses same index for both platforms
  constexpr ColorIndex(uint8_t value) : dos(value), amiga(std::nullopt) {}

  // Two value constructor - separate DOS and Amiga indices
  constexpr ColorIndex(uint8_t dos_value, uint8_t amiga_value)
      : dos(dos_value), amiga(amiga_value) {}

  [[nodiscard]] uint8_t get() const;
};

} // namespace gfx
