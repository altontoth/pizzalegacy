/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>
#include "core/common/Ids.h"
#include "core/state/UIState.h"
#include "engine/LayerInfo.h"
#include "resources/util/FileUtils.h"
#include "views/base/Menu.h"

class RecipeFileInfoBox;
class RecipePreview;

class MenuRecipeImport : public Menu {
public:
  MenuRecipeImport(GameContext& context, GameState& state, RecipeImportState s);
  ~MenuRecipeImport() override;

  std::vector<LayerInfo> get_layers() override;

private:
  Surface background() const override;
  void click_file(const std::string& filename);
  void import_marked();

  core::PlayerId player_;
  std::vector<fileutils::FileEntry> files_;
  std::set<std::string> marked_;
  std::set<std::string> unreadable_;
  // The file in the info box
  std::string selected_;
  bool select_several_ = false;
  int scroll_offset_ = 0;
  RecipeFileInfoBox* info_box_ = nullptr;
  std::unique_ptr<RecipePreview> preview_;
};
