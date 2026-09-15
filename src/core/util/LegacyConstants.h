/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

namespace LegacyConstants {

// Wood border dimensions from original game
// These affect coordinate transforms when loading original data files
inline constexpr int WOOD_BORDER_HEIGHT_TOP = 28;
inline constexpr int WOOD_BORDER_HEIGHT_BOTTOM = 20;
inline constexpr int WOOD_BORDER_WIDTH_LEFT = 24;
inline constexpr int WOOD_BORDER_WIDTH_RIGHT = 28;

// Restaurant tile dimensions from original game
inline constexpr int RESTAURANT_TILE_WIDTH = 24;
inline constexpr int RESTAURANT_TILE_HEIGHT = 22;
// Restaurant tiles on screen at once
inline constexpr int RESTAURANT_VISIBLE_X_TILES = 16;
inline constexpr int RESTAURANT_VISIBLE_Y_TILES = 14;

// The original biases restaurant Y coordinates by this much.
inline constexpr int RESTAURANT_LEGACY_Y_BIAS = 176;

// Map tile dimensions from original game (before zoom scaling)
inline constexpr int BASE_MAP_TILE_WIDTH = 8;
inline constexpr int BASE_MAP_TILE_HEIGHT = 8;

} // namespace LegacyConstants
