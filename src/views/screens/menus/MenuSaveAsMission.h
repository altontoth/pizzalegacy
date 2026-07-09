/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>
#include "resources/util/FileUtils.h"
#include "views/base/Menu.h"

// Save the active player's career (CV) as a playable .mis mission file,
// reached via the disk menu (Save -> as mission).
class MenuSaveAsMission : public Menu {
public:
  MenuSaveAsMission(GameContext& context, GameState& state);

private:
  Surface background() const override;
  void save_to_file(const std::string& filename);
  void write_mission(const std::string& filename);

  std::vector<fileutils::FileEntry> mission_files_;
  int scroll_offset_ = 0;
};
