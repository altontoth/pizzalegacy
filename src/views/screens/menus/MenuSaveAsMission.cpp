/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/MenuSaveAsMission.h"

#include <algorithm>
#include "core/character/Player.h"
#include "core/events/FullscreenMessageManager.h"
#include "core/mission/MissionFile.h"
#include "core/state/GameState.h"
#include "engine/Logger.h"
#include "resources/FontTypes.h"
#include "views/screens/menus/MenuTheme.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/AlertPopup.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/ui/widgets/ArrowMenuItem.h"
#include "views/ui/widgets/PositionMarkerMenuItem.h"
#include "views/ui/widgets/SaveFileInputBox.h"
#include "views/ui/widgets/TextMenuItem.h"

using namespace views::ui::menu;

#define SAVE_BOX_OFFSET_X 35
#define SAVE_BOX_OFFSET_Y 125
#define SAVE_BOX_W 224
#define SAVE_BOX_H 216
#define SAVE_BOX_ARROW_H 24

constexpr int MAX_VISIBLE_FILES = 12;

MenuSaveAsMission::MenuSaveAsMission(GameContext& context, GameState& state)
    : Menu(context, state, ViewPoint{SAVE_BOX_OFFSET_X, SAVE_BOX_OFFSET_Y}, 0) {

  mission_files_ = core::mission::list_player_mission_files();

  add_menu_item(std::make_unique<ArrowMenuItem>(context, ArrowType::Up, [this] {
    if (scroll_offset_ > 0) {
      scroll_offset_--;
      mark_dirty();
    }
  }));

  // Mission file input box (with NEW NAME entry + existing missions)
  add_menu_item(std::make_unique<SaveFileInputBox>(
      context, mission_files_, &scroll_offset_,
      [this](const std::string& filename) { save_to_file(filename); }, ".mis"));

  add_menu_item(
      std::make_unique<ArrowMenuItem>(context, ArrowType::Down, [this] {
        // Total items: 1 (NEW NAME) + mission_files_.size()
        int max_scroll = std::max(0, static_cast<int>(mission_files_.size()) +
                                         1 - MAX_VISIBLE_FILES);
        if (scroll_offset_ < max_scroll) {
          scroll_offset_++;
          mark_dirty();
        }
      }));

  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 234, 248));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Back",
      [this] {
        // Pop twice to get past the in-between screen
        state_.ui().pop();
        state_.ui().pop();
      },
      FontFace::RUBY, style, sel_style, item_align));
}

void MenuSaveAsMission::save_to_file(const std::string& filename) {
  Logger::info("Mission save requested for: {}", filename);

  const auto* player = state_.active_player();
  if (!player ||
      state_.message_manager().get_cv_entries_for(player->id()).empty()) {
    context_.popup->origin(ViewPoint{26, 40});
    context_.popup->create<AlertPopup>(
        "Your career is still empty - achieve something first!");
    return;
  }

  bool file_exists =
      std::ranges::any_of(mission_files_, [&](const fileutils::FileEntry& f) {
        return f.filename == filename;
      });

  if (file_exists) {
    context_.popup->origin(ViewPoint{26, 40});
    context_.popup->create<ConfirmPopup>("Overwrite old file?",
                                         [this, filename](bool confirmed) {
                                           if (confirmed) {
                                             write_mission(filename);
                                           }
                                         });
  }
  else {
    write_mission(filename);
  }
}

void MenuSaveAsMission::write_mission(const std::string& filename) {
  // The input box guarantees the .mis suffix; the writer appends it again.
  std::string name = filename;
  if (name.ends_with(".mis")) {
    name = name.substr(0, name.size() - 4);
  }
  name = core::mission::sanitize_mission_name(name);
  if (name.empty()) {
    name = "career";
  }

  const auto* player = state_.active_player();
  const auto entries =
      state_.message_manager().get_cv_entries_for(player->id());
  if (core::mission::save_career_mission(name, entries)) {
    Logger::info("Saved career as mission: {}", name);
    // Pop twice to get back to DiskMenu (not just to the save menu)
    state_.ui().pop();
    state_.ui().pop();
  }
  else {
    Logger::warn("Failed to save mission: {}", name);
    context_.popup->origin(ViewPoint{26, 40});
    context_.popup->create<AlertPopup>("Failed to save the mission file.");
  }
}

Surface MenuSaveAsMission::background() const {
  auto background = default_background("Mission save");

  // Gray shadow box (same as MenuSaveAsGame)
  ViewRect save_box(SAVE_BOX_OFFSET_X - 2, SAVE_BOX_OFFSET_Y + 4, SAVE_BOX_W,
                    SAVE_BOX_H + (SAVE_BOX_ARROW_H * 2) + 2);
  background.fill_rect(save_box, 14);

  ViewRect save_box2(SAVE_BOX_OFFSET_X, SAVE_BOX_OFFSET_Y, SAVE_BOX_W + 2,
                     SAVE_BOX_H + (SAVE_BOX_ARROW_H * 2));
  background.fill_rect(save_box2, 13);

  return background;
}
