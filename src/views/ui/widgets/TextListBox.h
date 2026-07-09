/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <string>
#include <vector>
#include "engine/DebugLabel.h"
#include "engine/geom/Spaces.h"
#include "views/ui/widgets/MenuItem.h"

// A scrollable box of text lines. Lines can be made clickable (hover highlight
// + click callback with the line index) or display-only. Scrolling advances one
// line at a time.
class TextListBox : public MenuItem, public debug::AutoLabel<TextListBox> {
public:
  TextListBox(GameContext& gc, Size size, int* scroll_offset,
              std::function<void(int)> on_line_clicked = nullptr)
      : MenuItem(gc), size_(size), scroll_offset_(scroll_offset),
        on_line_clicked_(std::move(on_line_clicked)) {}

  void set_lines(std::vector<std::string> lines, bool clickable);
  int max_scroll() const;

  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override {}
  Size measure() const override { return size_; }
  bool handles_mouse_events() override { return true; }
  bool on_interaction(views::ui::InteractionEvent ie) override;

private:
  int visible_lines() const;
  void draw_scroll_thumb(Surface& block, int total_lines) const;

  Size size_;
  std::vector<std::string> lines_;
  bool clickable_ = false;
  int* scroll_offset_;
  std::function<void(int)> on_line_clicked_;
  int hovered_index_ = -1;
};
