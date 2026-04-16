/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/MenuSaveAsGame.h"

#include <algorithm>
#include "core/save/SaveGameManager.h"
#include "core/state/GameState.h"
#include "engine/Logger.h"
#include "resources/util/FileUtils.h"
#include "views/screens/menus/MenuTheme.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/ui/widgets/ArrowMenuItem.h"
#include "views/ui/widgets/PositionMarkerMenuItem.h"
#include "views/ui/widgets/SaveFileInputBox.h"
#include "views/ui/widgets/SpacerMenuItem.h"
#include "views/ui/widgets/TextMenuItem.h"

using namespace views::ui::menu;

#define SAVE_BOX_OFFSET_X 35
#define SAVE_BOX_OFFSET_Y 125
#define SAVE_BOX_W 224
#define SAVE_BOX_H 216
#define SAVE_BOX_ARROW_H 24

constexpr int MAX_VISIBLE_FILES = 12;

MenuSaveAsGame::MenuSaveAsGame(GameContext& context, GameState& state)
    : Menu(context, state, ViewPoint{SAVE_BOX_OFFSET_X, SAVE_BOX_OFFSET_Y}, 0) {

  load_save_files();

  // Up arrow to scroll up in the list
  add_menu_item(std::make_unique<ArrowMenuItem>(context, ArrowType::Up, [this] {
    if (scroll_offset_ > 0) {
      scroll_offset_--;
      mark_dirty();
    }
  }));

  // Save file input box (with NEW NAME entry + existing saves)
  add_menu_item(std::make_unique<SaveFileInputBox>(
      context, save_files_, &scroll_offset_,
      [this](const std::string& filename) { save_to_file(filename); }));

  // Down arrow to scroll down in the list
  add_menu_item(
      std::make_unique<ArrowMenuItem>(context, ArrowType::Down, [this] {
        // Total items: 1 (NEW NAME) + save_files_.size()
        int max_scroll = std::max(0, static_cast<int>(save_files_.size()) + 1 -
                                         MAX_VISIBLE_FILES);
        if (scroll_offset_ < max_scroll) {
          scroll_offset_++;
          mark_dirty();
        }
      }));

  // Back button
  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 234, 248));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Back",
      [this] {
        // Pop twice to get past the in-between screen
        state_.ui().pop();
        state_.ui().pop();
      },
      FontFace::RUBY, style, sel_style));
}

void MenuSaveAsGame::load_save_files() {
  auto sort = sort_by_date_ ? fileutils::FileSortOrder::LastModified
                            : fileutils::FileSortOrder::Alphabetical;
  save_files_ = fileutils::list_files("saves", ".plsave", sort);
}

void MenuSaveAsGame::save_to_file(const std::string& filename) {
  Logger::info("Save requested for: {}", filename);

  // Check if file already exists
  bool file_exists = false;
  for (const auto& existing_file : save_files_) {
    if (existing_file.filename == filename) {
      file_exists = true;
      break;
    }
  }

  if (file_exists) {
    // Show confirmation popup
    save_to_existing_file(filename);
  }
  else {
    // New file, save directly
    Logger::info("Saving new file: {}", filename);
    if (save::SaveGameManager::save(state_, context_, filename)) {
      Logger::info("Successfully saved: {}", filename);
      // Pop twice to get back to DiskMenu (not just to the save menu)
      state_.ui().pop();
      state_.ui().pop();
    }
    else {
      Logger::warn("Failed to save: {}", filename);
      // TODO: Show error popup to user
    }
  }
}

void MenuSaveAsGame::save_to_existing_file(const std::string& filename) {
  context_.popup->palette_handle({38});
  context_.popup->origin(ViewPoint{26, 40});
  context_.popup->create<ConfirmPopup>(
      "Overwrite old file?", [this, filename](bool confirmed) {
        if (confirmed) {
          Logger::info("Overwriting file: {}", filename);
          if (save::SaveGameManager::save(state_, context_, filename)) {
            Logger::info("Successfully saved: {}", filename);
            // Pop twice to get back to DiskMenu (not just to the save menu)
            state_.ui().pop();
            state_.ui().pop();
          }
          else {
            Logger::warn("Failed to save: {}", filename);
            // TODO: Show error popup to user
          }
        }
        else {
          Logger::info("Save cancelled by user");
        }
      });
}

Surface MenuSaveAsGame::background() const {
  auto background = default_background("Game save");

  // Gray shadow box (same as GameLoad and CharacterSelection)
  ViewRect save_box(SAVE_BOX_OFFSET_X - 2, SAVE_BOX_OFFSET_Y + 4, SAVE_BOX_W,
                    SAVE_BOX_H + (SAVE_BOX_ARROW_H * 2) + 2);
  background.fill_rect(save_box, 14);

  ViewRect save_box2(SAVE_BOX_OFFSET_X, SAVE_BOX_OFFSET_Y, SAVE_BOX_W + 2,
                     SAVE_BOX_H + (SAVE_BOX_ARROW_H * 2));
  background.fill_rect(save_box2, 13);

  return background;
}
