/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/util/Constants.h"
#include "core/util/LegacyConstants.h"
#include "views/base/ViewDefs.h"
#include "views/screens/city_view/CityViewCommon.h"

constexpr int ZOOM_BOX_WIDTH_MEDIUM = 96;
constexpr int ZOOM_BOX_HEIGHT_MEDIUM = 88;
constexpr int ZOOM_BOX_WIDTH_FAR = 80;
constexpr int ZOOM_BOX_HEIGHT_FAR = 60;
constexpr int ZOOM_BOX_BORDER_WIDTH = 2;

struct ZoomConfig {
  int tile_width;
  int tile_height;
  int viewport_width;
  int viewport_height;
  int scroll_max_x;
  int scroll_max_y;
  int zoom_multiplier;
  int x_borders_offset;
  int y_borders_offset;
  int zoom_box_w;
  int zoom_box_h;
  float scroll_speed_multiplier;

  static ZoomConfig for_zoom_level(ZoomLevel zoom) {
    using namespace views::base;
    using namespace LegacyConstants;
    switch (zoom) {
    case ZoomLevel::FAR:
      return {.tile_width = 2,
              .tile_height = 2,
              .viewport_width = TABBED_VIEWPORT_WIDTH,
              .viewport_height = TABBED_VIEWPORT_HEIGHT,
              .scroll_max_x = MAP_TILES_PER_ROW * 2,
              .scroll_max_y = MAP_TILES_PER_COLUMN * 2,
              .zoom_multiplier = 16,
              .x_borders_offset = TABBED_VIEW_OFFSET_X,
              .y_borders_offset = TABBED_VIEW_OFFSET_Y,
              .zoom_box_w = ZOOM_BOX_WIDTH_FAR,
              .zoom_box_h = ZOOM_BOX_HEIGHT_FAR,
              .scroll_speed_multiplier = 1.0f}; // unused
    case ZoomLevel::MEDIUM:
      return {.tile_width = BASE_MAP_TILE_WIDTH,
              .tile_height = BASE_MAP_TILE_HEIGHT,
              .viewport_width = TABBED_VIEWPORT_WIDTH,
              .viewport_height = TABBED_VIEWPORT_HEIGHT,
              .scroll_max_x = MAP_TILES_PER_ROW * BASE_MAP_TILE_WIDTH,
              .scroll_max_y = MAP_TILES_PER_COLUMN * BASE_MAP_TILE_HEIGHT,
              .zoom_multiplier = 4,
              .x_borders_offset = TABBED_VIEW_OFFSET_X,
              .y_borders_offset = TABBED_VIEW_OFFSET_Y,
              .zoom_box_w = ZOOM_BOX_WIDTH_MEDIUM,
              .zoom_box_h = ZOOM_BOX_HEIGHT_MEDIUM,
              .scroll_speed_multiplier = 8.0f};

    case ZoomLevel::CLOSE:
      return {.tile_width = BASE_MAP_TILE_WIDTH * BASE_MAP_ZOOM_MULTIPLIER,
              .tile_height = BASE_MAP_TILE_HEIGHT * BASE_MAP_ZOOM_MULTIPLIER,
              .viewport_width = MAIN_PANEL_WIDTH,
              .viewport_height = MAIN_PANEL_HEIGHT,
              .scroll_max_x = MAP_TILES_PER_ROW * BASE_MAP_TILE_WIDTH *
                              BASE_MAP_ZOOM_MULTIPLIER,
              .scroll_max_y = MAP_TILES_PER_COLUMN * BASE_MAP_TILE_HEIGHT *
                              BASE_MAP_ZOOM_MULTIPLIER,
              .zoom_multiplier = 1,
              .x_borders_offset = 0,
              .y_borders_offset = 0,
              .zoom_box_w = 0,
              .zoom_box_h = 0,
              .scroll_speed_multiplier = 2.0f};
    }
    throw std::invalid_argument("Invalid zoom_ level");
  }
};
