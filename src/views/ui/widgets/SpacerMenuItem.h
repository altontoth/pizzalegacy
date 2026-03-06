/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/geom/Spaces.h"
#include "views/ui/widgets/MenuItem.h"

class SpacerMenuItem : public MenuItem {
public:
  SpacerMenuItem(GameContext& gc, int height) : MenuItem(gc) {
    height_ = height;
  }
  void draw(Surface& screen, ViewPoint p, bool is_selected) override {};
  void handle_action() override {};
  Size measure() const override { return Size{0, height_}; }
};
