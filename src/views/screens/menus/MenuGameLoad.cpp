/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/MenuGameLoad.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include "core/save/SaveGameManager.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "engine/Logger.h"
#include "resources/FontTypes.h"
#include "resources/util/FileUtils.h"
#include "views/screens/menus/MenuTheme.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/AlertPopup.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/ui/widgets/ArrowMenuItem.h"
#include "views/ui/widgets/PositionMarkerMenuItem.h"
#include "views/ui/widgets/SaveFileInfoBox.h"
#include "views/ui/widgets/SaveFileListBox.h"
#include "views/ui/widgets/TextMenuItem.h"

using namespace views::ui::menu;

#define SAVE_BOX_OFFSET_X 35
#define SAVE_BOX_OFFSET_Y 125
#define SAVE_BOX_W 224
#define SAVE_BOX_H 216
#define SAVE_BOX_ARROW_H 24

constexpr int MAX_VISIBLE_FILES = 12;

MenuGameLoad::MenuGameLoad(GameContext& context, GameState& state)
    : Menu(context, state, ViewPoint{SAVE_BOX_OFFSET_X, SAVE_BOX_OFFSET_Y}, 0) {

  load_save_files();

  // Up arrow to scroll up in the list
  add_menu_item(std::make_unique<ArrowMenuItem>(context, ArrowType::Up, [this] {
    if (scroll_offset_ > 0) {
      scroll_offset_--;
      mark_dirty();
    }
  }));

  // Save file list box - click selects, double-click loads directly
  add_menu_item(std::make_unique<SaveFileListBox>(
      context, save_files_, &scroll_offset_,
      [this](const std::string& filename) { select_file(filename); },
      [this](const std::string& filename) {
        select_file(filename);
        load_selected_file();
      }));

  // Down arrow to scroll down in the list
  add_menu_item(
      std::make_unique<ArrowMenuItem>(context, ArrowType::Down, [this] {
        int max_scroll = std::max(0, static_cast<int>(save_files_.size()) -
                                         MAX_VISIBLE_FILES);
        if (scroll_offset_ < max_scroll) {
          scroll_offset_++;
          mark_dirty();
        }
      }));

  // Position marker for info box column
  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 230, 0));

  auto info = std::make_unique<SaveFileInfoBox>(context);
  info_box_ = info.get();
  add_menu_item(std::move(info));

  constexpr TextStyle style{
      .color = 1,
      .align = AlignX::Left,
      .shadow = std::nullopt,
  };
  constexpr TextStyle sel_style{
      .color = 29,
      .align = AlignX::Left,
      .shadow = std::nullopt,
  };

  // Load button
  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 260, 160));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Load", [this] { load_selected_file(); }, face, style,
      sel_style));

  // Delete button
  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 440, 160));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Delete", [this] { delete_selected_file(); }, face, style,
      sel_style));

  // Sort toggle
  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 244, 244));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, compat::format("Sort: Name / Date"),
      [this] {
        sort_by_date_ = !sort_by_date_;
        load_save_files();
        mark_dirty();
      },
      FontFace::SUPERSMA, style, sel_style));
  // Back button
  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 510, 244));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Back", [this] { state_.ui().pop(); }, FontFace::RUBY, style,
      sel_style));
}

void MenuGameLoad::load_save_files() {
  auto sort = sort_by_date_ ? fileutils::FileSortOrder::LastModified
                            : fileutils::FileSortOrder::Alphabetical;
  save_files_ = fileutils::list_files("saves", ".plsave", sort);
}

void MenuGameLoad::select_file(const std::string& filename) {
  selected_file_ = filename;
  if (info_box_) {
    info_box_->set_file(filename);
  }
  mark_dirty();
}

void MenuGameLoad::load_selected_file() {
  if (selected_file_.empty()) {
    return;
  }

  Logger::info("Loading save file: {}", selected_file_);

  if (save::SaveGameManager::load(state_, context_, selected_file_)) {
    Logger::info("Successfully loaded: {}", selected_file_);
    state_.ui().replace_top<CityViewState>(state_.active_city_id());
  }
  else {
    Logger::warn("Failed to load: {}", selected_file_);
    context_.popup->create<AlertPopup>("Failed to load save file.");
  }
}

void MenuGameLoad::delete_selected_file() {
  if (selected_file_.empty()) {
    return;
  }

  std::string file_to_delete = selected_file_;
  context_.popup->create<ConfirmPopup>(
      "Do you want to delete this file?", [this, file_to_delete](bool agree) {
        if (!agree) {
          return;
        }

        namespace fs = std::filesystem;
        auto full_path =
            fs::path(save::SaveGameManager::get_saves_directory()) /
            file_to_delete;

        try {
          if (fs::remove(full_path)) {
            Logger::info("Deleted save file: {}", file_to_delete);
          }
          else {
            Logger::warn("File not found: {}", file_to_delete);
          }
        }
        catch (const std::exception& e) {
          Logger::warn("Failed to delete: {}", e.what());
        }

        selected_file_.clear();
        if (info_box_) {
          info_box_->clear();
        }
        load_save_files();
        scroll_offset_ = 0;
        mark_dirty();
      });
}

Surface MenuGameLoad::background() const {
  auto background = default_background("Game load");

  // Gray shadow box (same as CharacterSelection)
  ViewRect save_box(SAVE_BOX_OFFSET_X - 2, SAVE_BOX_OFFSET_Y + 4, SAVE_BOX_W,
                    SAVE_BOX_H + (SAVE_BOX_ARROW_H * 2) + 2);
  background.fill_rect(save_box, 14);

  ViewRect save_box2(SAVE_BOX_OFFSET_X, SAVE_BOX_OFFSET_Y, SAVE_BOX_W + 2,
                     SAVE_BOX_H + (SAVE_BOX_ARROW_H * 2));
  background.fill_rect(save_box2, 13);

  return background;
}
