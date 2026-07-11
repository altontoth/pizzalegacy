/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/geom/Spaces.h"
#include "views/ui/widgets/MenuItem.h"

enum class ArrowType { Up, Down };

class ArrowMenuItem : public MenuItem {
public:
  ArrowMenuItem(GameContext& gc, ArrowType direction,
                std::function<void()> action, std::string tooltip = "");
  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override;
  Size measure() const override;
  // Arrows always go above/below a box that exposes the same adjustment via
  // on_adjust(), so keyboard focus goes to the box instead.
  bool focusable() const override { return false; }
  std::string tooltip_text() const override { return tooltip_text_; }

private:
  std::function<void()> action_;
  Bob& menu_bobs_;
  std::string tooltip_text_;
  Surface arrow_surface_;
};
