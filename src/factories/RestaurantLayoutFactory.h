/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <span>

#include "core/restaurant/RestaurantLayout.h"

class RestaurantLayoutFactory {
public:
  static std::unique_ptr<RestaurantLayout> create_layout(const std::string&);

  // Parse a layout from an in-memory buffer in the original layout-file format.
  static std::unique_ptr<RestaurantLayout>
  create_layout(std::span<const uint8_t> bytes);
};
