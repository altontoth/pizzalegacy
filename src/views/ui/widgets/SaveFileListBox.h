/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
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
#include "resources/util/FileUtils.h"
#include "views/ui/widgets/ListCursor.h"
#include "views/ui/widgets/MenuItem.h"
#include "views/ui/widgets/ScrollBar.h"

class SaveFileListBox : public MenuItem,
                        public debug::AutoLabel<SaveFileListBox> {
public:
  SaveFileListBox(
      GameContext& gc, std::vector<fileutils::FileEntry>& save_files,
      int* scroll_offset,
      std::function<void(const std::string&)> on_file_clicked,
      std::function<void(const std::string&)> on_file_double_clicked = nullptr,
      std::string extension = ".plsave")
      : MenuItem(gc), save_files_(save_files), scroll_offset_(scroll_offset),
        on_file_clicked_(std::move(on_file_clicked)),
        on_file_double_clicked_(std::move(on_file_double_clicked)),
        extension_(std::move(extension)) {}

  // Rows is_marked accepts are highlighted.
  void mark_rows(std::function<bool(const std::string&)> is_marked) {
    is_marked_ = std::move(is_marked);
  }

  // Rows is_disabled accepts are greyed out and ignore clicks.
  void disable_rows(std::function<bool(const std::string&)> is_disabled) {
    is_disabled_ = std::move(is_disabled);
  }

  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override;
  Size measure() const override;
  bool handles_mouse_events() override { return true; }
  bool on_adjust(AdjustAxis axis, int delta) override;
  bool adjust_repeats() const override { return true; }
  bool on_interaction(views::ui::InteractionEvent ie) override;
  std::optional<views::ui::MouseButton> drag_button() const override {
    return views::ui::MouseButton::Left;
  }

private:
  int file_at(int local_y) const;
  bool disabled(int index) const;
  ScrollBar::Extent scroll_extent() const;

  std::vector<fileutils::FileEntry>& save_files_;
  int* scroll_offset_;
  std::function<void(const std::string&)> on_file_clicked_;
  std::function<void(const std::string&)> on_file_double_clicked_;
  std::string extension_;
  std::function<bool(const std::string&)> is_marked_;
  std::function<bool(const std::string&)> is_disabled_;
  ListCursor cursor_;
  ScrollBar scroll_bar_;
};
