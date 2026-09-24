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

class SaveFileInputBox : public MenuItem,
                         public input::InputListener,
                         public debug::AutoLabel<SaveFileInputBox> {
public:
  SaveFileInputBox(GameContext& gc,
                   std::vector<fileutils::FileEntry>& save_files,
                   int* scroll_offset,
                   std::function<void(const std::string&)> on_save_callback,
                   std::string extension = ".plsave")
      : MenuItem(gc), save_files_(save_files), scroll_offset_(scroll_offset),
        on_save_callback_(std::move(on_save_callback)),
        extension_(std::move(extension)) {}

  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override;
  Size measure() const override;
  bool handles_keyboard_events() override { return keyboard_enabled_; }
  bool handles_mouse_events() override { return true; }
  bool on_adjust(AdjustAxis axis, int delta) override;
  bool adjust_repeats() const override { return true; }
  bool on_interaction(views::ui::InteractionEvent ie) override;
  void begin_keyboard_session(input::InputManager* input,
                              std::function<void()> inv) override;

  bool on_text(const input::TextEvent& ev) override;
  bool on_action(const input::ActionEvent& ev) override;

  void end_input();
  void start_new_name_input(std::string initial = {});

private:
  void mark_dirty();
  int entry_at(int local_y) const;

  std::vector<fileutils::FileEntry>& save_files_;
  int* scroll_offset_;
  std::function<void(const std::string&)> on_save_callback_;
  // Appended to typed names and stripped from listed ones (".plsave",
  // ".mis", ...)
  std::string extension_;

  bool input_active_ = false;
  bool keyboard_enabled_ = false;
  std::string new_filename_;
  std::optional<input::TextSession> text_session_;
  std::function<void()> invalidate_;
  ListCursor cursor_;
};
