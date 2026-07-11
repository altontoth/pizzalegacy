/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include "core/save/SaveGameManager.h"
#include "engine/DebugLabel.h"
#include "engine/geom/Spaces.h"
#include "views/ui/widgets/MenuItem.h"

class SaveFileInfoBox : public MenuItem,
                        public debug::AutoLabel<SaveFileInfoBox> {
public:
  SaveFileInfoBox(GameContext& gc) : MenuItem(gc) {}

  void set_file(const std::string& filename);
  void clear();
  bool has_file() const { return has_info_; }

  void draw(Surface& screen, ViewPoint p, bool is_selected) override;
  void handle_action() override;
  Size measure() const override;
  bool focusable() const override { return false; }
  bool handles_mouse_events() override { return true; }
  bool on_interaction(views::ui::InteractionEvent ie) override;

private:
  bool has_info_ = false;
  bool file_attempted_ = false;
  save::SaveMetadata info_;
};
