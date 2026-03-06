/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <variant>
#include "core/city/CityTypes.h"
#include "core/common/Ids.h"

enum class ZoomLevel {
  CLOSE,
  MEDIUM,
  FAR,
};

enum class HighlightType {
  NONE,
  DOT,   // yellow dot for most highlights
  RANGE, // white diamond for player range
};

enum class CityViewTabType {
  MAP = 0,
  DISTRICTS,
  CONTROL,
};

enum class HighlightMode {
  NONE,
  PLAYER,
  PLAYER_RANGE,
  BUSINESS_PROPERTIES,
  PROPERTY_STATUS,
};

struct CityViewHighlight {
  HighlightMode mode = HighlightMode::NONE;

  std::variant<std::monostate, BuildingType, core::PlayerId, PremiseStatus>
      data;
};
