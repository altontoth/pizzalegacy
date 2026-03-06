/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <string>
#include "core/util/Constants.h"
#include "engine/DebugLabel.h"
#include "engine/geom/Spaces.h"
#include "views/ui/widgets/MenuItem.h"

struct SaveFileInfo {
  std::string filename;
  std::string saved_at;
  std::string game_date;
  std::array<std::string, NUMBER_OF_PLAYERS> player_names;
  int active_player = -1;
  int active_city = -1;
};

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
  bool handles_mouse_events() override { return true; }
  bool on_interaction(views::ui::InteractionEvent ie) override;

private:
  bool has_info_ = false;
  SaveFileInfo info_;
};
