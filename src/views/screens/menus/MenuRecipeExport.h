/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>
#include "core/common/Ids.h"
#include "core/state/UIState.h"
#include "resources/util/FileUtils.h"
#include "views/base/Menu.h"

namespace core::food {
class Recipe;
}

// Save one of the player's recipes as a file in the recipes directory. The
// name box starts on a free name made from the recipe's, so Enter saves it.
class MenuRecipeExport : public Menu {
public:
  MenuRecipeExport(GameContext& context, GameState& state, RecipeExportState s);

private:
  Surface background() const override;
  const core::food::Recipe* recipe() const;
  void save_to_file(const std::string& filename);
  void write_recipe(const std::string& filename);

  core::PlayerId player_;
  core::RecipeId recipe_;
  std::vector<fileutils::FileEntry> files_;
  int scroll_offset_ = 0;
};
