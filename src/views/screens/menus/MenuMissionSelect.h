/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include <optional>
#include <vector>
#include "core/mission/MissionTarget.h"
#include "resources/util/FileUtils.h"
#include "views/base/Menu.h"

class TextListBox;

class MenuMissionSelect : public Menu {
public:
  MenuMissionSelect(GameContext& context, GameState& state);

private:
  enum class GoalsBoxMode { Goals, Files };

  Surface background() const override;
  void select_named_mission(int index);
  void select_player_mission(int index);
  void load_mission(const std::filesystem::path& path);
  void toggle_disk();
  void show_goals();
  void show_files();
  void confirm();

  std::vector<fileutils::FileEntry> player_files_;
  std::optional<core::mission::MissionTarget> selected_;
  GoalsBoxMode goals_mode_ = GoalsBoxMode::Goals;
  int goals_scroll_ = 0;
  TextListBox* goals_box_ = nullptr;
};
