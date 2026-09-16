/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "engine/DebugLabel.h"
#include "engine/geom/Spaces.h"
#include "views/ui/widgets/ListCursor.h"
#include "views/ui/widgets/MenuItem.h"
#include "views/ui/widgets/ScrollBar.h"

// A scrollable box of text lines. Lines can be made clickable (hover highlight
// + click callback with the line index) or display-only. Scrolling advances one
// line at a time.
class TextListBox : public MenuItem, public debug::AutoLabel<TextListBox> {
public:
  TextListBox(GameContext& gc, Size size, int* scroll_offset, bool scroll_thumb,
              std::function<void(int)> on_line_clicked = nullptr)
      : MenuItem(gc), size_(size), scroll_offset_(scroll_offset),
        scroll_thumb_(scroll_thumb),
        on_line_clicked_(std::move(on_line_clicked)) {}

  // The placeholder is shown in light gray while the list is empty.
  void set_lines(std::vector<std::string> lines, bool clickable,
                 std::string placeholder = "");
  int max_scroll() const;

  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override;
  Size measure() const override { return size_; }
  bool handles_mouse_events() override { return true; }
  bool on_adjust(AdjustAxis axis, int delta) override;
  bool adjust_repeats() const override { return true; }
  bool on_interaction(views::ui::InteractionEvent ie) override;
  std::optional<views::ui::MouseButton> drag_button() const override {
    if (!scroll_thumb_) {
      return std::nullopt;
    }
    return views::ui::MouseButton::Left;
  }

private:
  int visible_lines() const;
  ScrollBar::Extent scroll_extent() const;
  int line_at(int local_y) const;

  Size size_;
  std::vector<std::string> lines_;
  std::string placeholder_;
  bool clickable_ = false;
  int* scroll_offset_;
  bool scroll_thumb_;
  std::function<void(int)> on_line_clicked_;
  ListCursor cursor_;
  ScrollBar scroll_bar_;
};
