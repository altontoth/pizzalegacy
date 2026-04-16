/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>
#include "resources/util/FileUtils.h"
#include "views/base/Menu.h"

class MenuSaveAsGame : public Menu {
public:
  MenuSaveAsGame(GameContext& context, GameState& state);

private:
  Surface background() const override;
  void load_save_files();
  void save_to_file(const std::string& filename);
  void save_to_existing_file(const std::string& filename);

  std::vector<fileutils::FileEntry> save_files_;
  int scroll_offset_ = 0;
  bool sort_by_date_ = true;
};
