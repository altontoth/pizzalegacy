/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>
#include "engine/DebugLabel.h"
#include "engine/geom/Spaces.h"
#include "resources/util/FileUtils.h"
#include "views/ui/widgets/MenuItem.h"

class SaveFileListBox : public MenuItem,
                        public debug::AutoLabel<SaveFileListBox> {
public:
  SaveFileListBox(
      GameContext& gc, std::vector<fileutils::FileEntry>& save_files,
      int* scroll_offset,
      std::function<void(const std::string&)> on_file_clicked,
      std::function<void(const std::string&)> on_file_double_clicked = nullptr)
      : MenuItem(gc), save_files_(save_files), scroll_offset_(scroll_offset),
        on_file_clicked_(std::move(on_file_clicked)),
        on_file_double_clicked_(std::move(on_file_double_clicked)) {}

  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override;
  Size measure() const override;
  bool handles_mouse_events() override { return true; }
  bool on_interaction(views::ui::InteractionEvent ie) override;

private:
  std::vector<fileutils::FileEntry>& save_files_;
  int* scroll_offset_;
  std::function<void(const std::string&)> on_file_clicked_;
  std::function<void(const std::string&)> on_file_double_clicked_;
  int hovered_index_ = -1;
};
