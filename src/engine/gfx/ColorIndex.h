/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace gfx {

struct ColorIndex {
  uint8_t dos;

  constexpr ColorIndex(uint8_t value) : dos(value) {}

  [[nodiscard]] uint8_t get() const;
};

} // namespace gfx
