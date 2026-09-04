/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <string>

#include "engine/geom/Spaces.h"
#include "resources/FontTypes.h"
#include "views/ui/widgets/MenuItem.h"

class CheckboxMenuItem : public MenuItem {
public:
  CheckboxMenuItem(GameContext& gc, const std::string& text, bool& value,
                   std::function<void()> action,
                   FontFace font_face = FontFace::SUPERSMA);

  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override;
  Size measure() const override;
  bool on_adjust(AdjustAxis axis, int delta) override;

private:
  bool& value_;
  std::function<void()> action_;
};
