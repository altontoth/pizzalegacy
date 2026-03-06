/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>
#include "views/base/Menu.h"

class MenuSaveAsGame : public Menu {
public:
  MenuSaveAsGame(GameContext& context, GameState& state);

private:
  Surface background() const override;
  void load_save_files();
  void save_to_file(const std::string& filename);
  void save_to_existing_file(const std::string& filename);

  std::vector<std::string> save_files_;
  int scroll_offset_ = 0;
};
