/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

// The first four match the original's two bit availability mask, bit 0 for sale
// and bit 1 to rent.
enum class PremiseStatus {
  NOT_AVAILABLE,
  FOR_SALE,
  FOR_RENT,
  FOR_SALE_RENT,
  OWNED,
  RENTED,
};

constexpr PremiseStatus premise_status_from_original(uint8_t value) {
  return static_cast<PremiseStatus>(value & 3);
}

enum class PropertyType {
  BUSINESS_PREMISE,
  DEPOT,
};

enum class BuildingType {
  CHURCH = 100,
  BANK,
  CINEMA,
  SCHOOL,
  DISCO,
  BAR,
  DEPARTMENT_STORE,
  HOTEL,
  RESTAURANT,
  DEPOT,
  BUSINESS_PREMISES,
  NONE,
};

enum class RoadType {
  NO_ROAD = 0x00,
  ROAD_W = 0x06,
  ROAD_E = 0x16,
  ROAD_S = 0x26,
  ROAD_N = 0x36,
  CORNER_SW = 0x46,
  CORNER_SE = 0x56,
  CORNER_NW = 0x66,
  CORNER_NE = 0x76,
  CORNER_W = 0x86,
  CORNER_N = 0x96,
  CORNER_E = 0xA6,
  CORNER_S = 0xB6,
  GLITCH_ROAD_E = 0xC6,
  GLITCH_ROAD_W = 0xD6,
  SHADOW_ROAD_N = 0x07,
  SHADOW_ROAD_W = 0x17,
  SHADOW_CORNER_NW = 0x27,
  PARTIAL_SHADOW_ROAD_N = 0x37,
  PARTIAL_SHADOW_ROAD_W = 0x47,
  SHADOW_CORNER_N = 0x57,
  SHADOW_CORNER_E = 0x67,
  SHADOW_CORNER_S = 0x87,
  SHADOW2_ROAD_E = 0x97,
  SHADOW2_ROAD_W = 0xA7,
  SHADOW_ZEBRA_ROAD_E = 0xB7,
  SHADOW_ZEBRA_ROAD_N = 0xC7,
  ZEBRA_ROAD_E = 0xD7,
  ZEBRA_ROAD_S = 0xE7,
  SHADOW_ROAD_S = 0xF7,
};
