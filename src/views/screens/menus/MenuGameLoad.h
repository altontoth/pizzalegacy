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
#include "views/ui/BusyCursor.h"

struct FileEntry;
class SaveFileInfoBox;

class MenuGameLoad : public Menu {
public:
  MenuGameLoad(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  Surface background() const override;
  void load_save_files();
  void reload_file_list();
  void remember_list_options();
  void select_file(const std::string& filename);
  void load_selected_file();
  void delete_selected_file();

  views::ui::BusyCursor busy_cursor_;
  std::vector<fileutils::FileEntry> save_files_;
  int scroll_offset_ = 0;
  std::string selected_file_;
  SaveFileInfoBox* info_box_ = nullptr;
};
