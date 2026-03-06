/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include "engine/geom/Spaces.h"
#include "gfx/Surface.h"
#include "views/screens/city_view/CityViewCommon.h"

using gfx::Surface;

class CityViewTab {
public:
  CityViewTab(CityViewTabType type, ViewRect area, std::size_t depth)
      : type_(type), depth_(depth), area_(area) {}

  CityViewTabType type() const { return type_; }
  ViewPoint position() const { return area_.point(); }
  std::size_t depth() const { return depth_; }
  ViewRect rect() const { return area_; }

  void position(ViewPoint p) {
    area_.x = p.x;
    area_.y = p.y;
  }

  void depth(std::size_t depth) { depth_ = depth; }
  Size size() const { return area_.size(); }

  std::string name() {
    switch (type_) {
    case CityViewTabType::MAP:
      return "MAP";
    case CityViewTabType::DISTRICTS:
      return "DISTRICTS";
    case CityViewTabType::CONTROL:
      return "CONTROL";
    default:
      return "Unknown";
    }
  }

private:
  CityViewTabType type_;
  std::size_t depth_;
  ViewRect area_;
};
