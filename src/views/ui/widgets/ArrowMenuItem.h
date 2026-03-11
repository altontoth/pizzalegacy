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
  std::string tooltip_text() const override { return tooltip_text_; }

private:
  std::function<void()> action_;
  Bob& menu_bobs_;
  Surface arrow_surface_;
  std::string tooltip_text_;
};
