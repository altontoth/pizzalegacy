/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/util/LegacyConstants.h"
#include "engine/geom/Spaces.h"

#include <cstdint>

// World map constants
inline constexpr int MAP_TILES_PER_ROW = 160;
inline constexpr int MAP_TILES_PER_COLUMN = 120;
inline constexpr int NUMBER_OF_DISTRICT_CELL_COLUMNS = 20;
inline constexpr int NUMBER_OF_DISTRICT_CELL_ROWS = 15;

// Map rendering constants
inline constexpr int BASE_MAP_ZOOM_MULTIPLIER = 4;
inline constexpr int LANDSYM_TILES_PER_ROW = 16; // 512/TILE_WIDTH

// Game constants
inline constexpr int NUMBER_OF_CUSTOMER_TYPES = 6;
inline constexpr int NUMBER_OF_CITIES = 10;
inline constexpr int NUMBER_OF_PLAYERS = 8;

// Source rectangle in the tile atlas for a given tile type.
// multiplier: 1 for medium zoom (8x8), BASE_MAP_ZOOM_MULTIPLIER for close
// (32x32).
inline ViewRect landsym_source_rect(uint8_t type, uint8_t multiplier = 1) {
  return ViewRect{(type / LANDSYM_TILES_PER_ROW) *
                      LegacyConstants::BASE_MAP_TILE_WIDTH * multiplier,
                  (type % LANDSYM_TILES_PER_ROW) *
                      LegacyConstants::BASE_MAP_TILE_HEIGHT * multiplier,
                  LegacyConstants::BASE_MAP_TILE_WIDTH * multiplier,
                  LegacyConstants::BASE_MAP_TILE_HEIGHT * multiplier};
}
