/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/util/LegacyConstants.h"

namespace views::base {

// Grid cell constants (for district & control views)
inline constexpr int NUMBER_OF_CELL_COLUMNS = 20;
inline constexpr int NUMBER_OF_CELL_ROWS = 15;

inline constexpr int MAIN_PANEL_WIDTH = 384;
inline constexpr int MAIN_PANEL_HEIGHT = 352;

inline constexpr int INPUT_PANEL_WIDTH = 166;
inline constexpr int INPUT_PANEL_HEIGHT = 154;
inline constexpr int INPUT_PANEL_OFFSET_X = 456;
inline constexpr int INPUT_PANEL_OFFSET_Y = 236;

inline constexpr int INPUT_PANEL_INNER_OFFSET_X = 0;
inline constexpr int INPUT_PANEL_INNER_OFFSET_Y = 20;

inline constexpr int STATUS_PANEL_WIDTH = 166;
inline constexpr int STATUS_PANEL_HEIGHT = 176;
inline constexpr int STATUS_PANEL_OFFSET_X = 456;
inline constexpr int STATUS_PANEL_OFFSET_Y = 24;

// For CityView we're inside the tabbed view:
inline constexpr int TABBED_VIEWPORT_WIDTH = 320;
inline constexpr int TABBED_VIEWPORT_HEIGHT = 240;
inline constexpr int TABBED_VISIBLE_TILES_X =
    TABBED_VIEWPORT_WIDTH / LegacyConstants::BASE_MAP_TILE_WIDTH;
inline constexpr int TABBED_VISIBLE_TILES_Y =
    TABBED_VIEWPORT_HEIGHT / LegacyConstants::BASE_MAP_TILE_HEIGHT;
inline constexpr int TABBED_VIEW_OFFSET_X = 40;
inline constexpr int TABBED_VIEW_OFFSET_Y = 94;

} // namespace views::base
