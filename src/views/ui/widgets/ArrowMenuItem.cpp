/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/ArrowMenuItem.h"
#include "engine/geom/Spaces.h"

ArrowMenuItem::ArrowMenuItem(GameContext& gc, ArrowType direction,
                             std::function<void()> action)
    : MenuItem(gc), action_(action),
      menu_bobs_(gc.bob->get("bob/menubobs.bob")),
      arrow_surface_(direction == ArrowType::Up
                         ? menu_bobs_.frame(3, PaletteHandle{63})
                         : menu_bobs_.frame(4, PaletteHandle{63})) {
  height_ = arrow_surface_.height();
  width_ = arrow_surface_.width();
}

void ArrowMenuItem::draw(Surface& screen, ViewPoint p, bool is_selected) {
  screen.blit_raw(arrow_surface_, p);
}

Size ArrowMenuItem::measure() const { return Size{width_, height_}; }

void ArrowMenuItem::handle_action() { action_(); }
