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
  // Prebuilt restaurants in f/, numbered from 1, that AI restaurants use.
  static constexpr int ai_layout_count = 24;

  static std::unique_ptr<RestaurantLayout> create_layout(const std::string&);

  // Parse a layout from an in-memory buffer in the original layout-file format.
  static std::unique_ptr<RestaurantLayout>
  create_layout(std::span<const uint8_t> bytes);

  // Prebuilt layout `index` with only its first `furniture_count` furniture
  // items placed.
  static std::unique_ptr<RestaurantLayout>
  create_ai_layout(int index, int furniture_count);
};
