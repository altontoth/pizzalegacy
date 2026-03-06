/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>
#include "views/base/Menu.h"

class SaveFileInfoBox;

class MenuGameLoad : public Menu {
public:
  MenuGameLoad(GameContext& context, GameState& state);

private:
  Surface background() const override;
  void load_save_files();
  void select_file(const std::string& filename);
  void load_selected_file();
  void delete_selected_file();

  std::vector<std::string> save_files_;
  int scroll_offset_ = 0;
  std::string selected_file_;
  SaveFileInfoBox* info_box_ = nullptr;
};
