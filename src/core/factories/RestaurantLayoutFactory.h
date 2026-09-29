/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "core/restaurant/RestaurantLayout.h"

class RestaurantLayoutFactory {
public:
  // Prebuilt restaurants in f/, numbered from 1, that AI restaurants use.
  static constexpr int ai_layout_count = 24;

  // A layout file is a fixed header ending in the two record counts, then the
  // furniture records and the oven records.
  static constexpr std::size_t header_size = 41;
  static constexpr std::size_t floor_offset = 9;
  static constexpr std::size_t furniture_count_offset = 39;
  static constexpr std::size_t oven_count_offset = 40;
  static constexpr std::size_t record_size = 9;

  static std::unique_ptr<RestaurantLayout> create_layout(const std::string&);

  // Parse a layout from an in-memory buffer in the original layout-file format.
  static std::unique_ptr<RestaurantLayout>
  create_layout(std::span<const uint8_t> bytes);

  // Prebuilt layout `index` with only its first `furniture_count` furniture
  // items placed.
  static std::unique_ptr<RestaurantLayout>
  create_ai_layout(int index, int furniture_count);
};
