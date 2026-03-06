/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>

class Furniture;

struct FurnitureBatch {
  const Furniture* furniture;
  int count;
  std::optional<int>
      bob_state; // Rotation state (only set when picking up existing furniture)
};
