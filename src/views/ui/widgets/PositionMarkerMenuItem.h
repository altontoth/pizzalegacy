/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/geom/Spaces.h"
#include "views/ui/widgets/MenuItem.h"

class PositionMarkerMenuItem : public MenuItem {
public:
  PositionMarkerMenuItem(GameContext& gc, int x_offset, int y_offset)
      : MenuItem(gc) {
    width_ = x_offset;
    height_ = y_offset;
  }
  // PositionMarkerMenuItem does not render anything but communicates
  // the wanted positions with width & height, so this is a noop
  void draw(Surface& screen, ViewPoint p, bool is_selected) override {}
  void handle_action() override {}
  Size measure() const override { return Size{width_, height_}; }
};
