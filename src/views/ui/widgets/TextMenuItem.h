/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/Geometry.h"
#include "engine/geom/Spaces.h"
#include "views/ui/widgets/MenuItem.h"

class TextMenuItem : public MenuItem {
public:
  TextMenuItem(GameContext& gc, const std::string& text,
               std::function<void()> action,
               FontFace font_face = FontFace::MENU, const TextStyle& ts = {},
               const TextStyle& sel_ts = {}, HAlign align = HAlign::Left)
      : MenuItem(gc, font_face, text, align), action_(std::move(action)),
        style_(ts), selected_style_(sel_ts) {}

  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override;
  Size measure() const override;

private:
  std::function<void()> action_;
  const TextStyle style_;
  const TextStyle selected_style_;
};
