/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>
#include "engine/DebugLabel.h"
#include "engine/geom/Spaces.h"
#include "views/ui/widgets/ListCursor.h"
#include "views/ui/widgets/MenuItem.h"

class MissionTypeBox : public MenuItem,
                       public debug::AutoLabel<MissionTypeBox> {
public:
  MissionTypeBox(GameContext& gc, Size size, std::vector<std::string> lines,
                 std::function<void(int)> on_line_clicked)
      : MenuItem(gc), size_(size), lines_(std::move(lines)),
        on_line_clicked_(std::move(on_line_clicked)) {}

  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override;
  Size measure() const override { return size_; }
  bool handles_mouse_events() override { return true; }
  bool on_adjust(AdjustAxis axis, int delta) override;
  bool adjust_repeats() const override { return true; }
  bool on_interaction(views::ui::InteractionEvent ie) override;

private:
  Size size_;
  std::vector<std::string> lines_;
  std::function<void(int)> on_line_clicked_;
  ListCursor cursor_;
};
