/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstdint>

enum class AwardCategory : uint8_t {
  Heart = 0,
  Pizza,
  Star,
  Spoon,
  BowTie,
  Cross,
  Count,
};

enum class AwardLevel : uint8_t {
  None = 0,
  Bronze = 1,
  Silver = 2,
  Gold = 3,
};

using Awards =
    std::array<AwardLevel, static_cast<size_t>(AwardCategory::Count)>;
