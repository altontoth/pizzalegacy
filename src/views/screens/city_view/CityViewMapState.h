/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include "core/common/Ids.h"
#include "core/util/Constants.h"
#include "engine/geom/Spaces.h"
#include "views/screens/city_view/CityViewCommon.h"
#include "views/screens/city_view/ZoomConfig.h"

struct CityViewMapState {
  std::optional<core::CityId> city_id;
  WorldPoint scroll = WorldPoint{-1, -1};
  ZoomLevel zoom = ZoomLevel::FAR;
  CityViewTabType active_tab = CityViewTabType::MAP;
  CityViewHighlight highlight{};

  // Default just highlights the player 0 restaurants:
  CityViewMapState(core::PlayerId active_player_id = {0}) {
    highlight = CityViewHighlight{
        .mode = HighlightMode::PLAYER,
        .data = active_player_id,
    };
  }

  // Update scroll so the camera will be centered on the given tile index
  // when returning to the city view.
  void center_on_index(int tile_index) {
    using namespace LegacyConstants;
    int tx = tile_index % MAP_TILES_PER_ROW;
    int ty = tile_index / MAP_TILES_PER_ROW;
    int wx = tx * BASE_MAP_TILE_WIDTH * BASE_MAP_ZOOM_MULTIPLIER;
    int wy = ty * BASE_MAP_TILE_HEIGHT * BASE_MAP_ZOOM_MULTIPLIER;

    auto cfg = ZoomConfig::for_zoom_level(zoom);
    scroll.x = wx - (cfg.viewport_width * cfg.zoom_multiplier) / 2;
    scroll.y = wy - (cfg.viewport_height * cfg.zoom_multiplier) / 2;
  }
};
