/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include "core/city/CityViewCommon.h"
#include "core/common/Ids.h"
#include "engine/geom/Spaces.h"

// Per-player city view state, persisted across screen transitions.
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
};
