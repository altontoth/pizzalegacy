/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/MenuMissionSelect.h"

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include "core/events/CareerLog.h"
#include "core/mission/MissionFile.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/PlatformPaths.h"
#include "resources/Font.h"
#include "resources/FontTypes.h"
#include "resources/util/FileUtils.h"
#include "views/screens/menus/MenuTheme.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/AlertPopup.h"
#include "views/ui/widgets/ArrowMenuItem.h"
#include "views/ui/widgets/MissionTypeBox.h"
#include "views/ui/widgets/PositionMarkerMenuItem.h"
#include "views/ui/widgets/SpacerMenuItem.h"
#include "views/ui/widgets/TextListBox.h"
#include "views/ui/widgets/TextMenuItem.h"

using namespace views::ui::menu;

#define GOALS_BOX_OFFSET_X 15
#define GOALS_BOX_OFFSET_Y 125
#define GOALS_BOX_W 224
#define GOALS_BOX_H 216
#define BOX_ARROW_H 24

// Middle button column and right-hand named mission box, relative to the
// menu origin (the goals box column)
constexpr int BUTTONS_X = -20;
constexpr int BUTTONS_Y = 0;
constexpr int BACK_BUTTON_Y_OFFSET = 140;
constexpr int NAMES_BOX_X = 370;
constexpr int NAMES_BOX_W = 224;

// The built-in missions, in m/<index>.mis order (original button order)
constexpr std::array<const char*, 8> NAMED_MISSIONS = {
    "Entry",  "Rise",      "Fall",   "Gangster",
    "Tricky", "Traveller", "Winner", "Short",
};

MenuMissionSelect::MenuMissionSelect(GameContext& context, GameState& state)
    : Menu(context, state, ViewPoint{GOALS_BOX_OFFSET_X, GOALS_BOX_OFFSET_Y},
           0) {

  // Left column: the selected mission's target CV (or the disk file list)
  add_menu_item(std::make_unique<ArrowMenuItem>(context, ArrowType::Up, [this] {
    if (goals_scroll_ > 0) {
      goals_scroll_--;
      mark_dirty();
    }
  }));

  auto goals = std::make_unique<TextListBox>(
      context, Size{GOALS_BOX_W, GOALS_BOX_H}, &goals_scroll_,
      state.rules().qol_improvements, [this](int index) {
        if (goals_mode_ == GoalsBoxMode::Files) {
          select_player_mission(index);
        }
      });
  goals_box_ = goals.get();
  add_menu_item(std::move(goals));

  add_menu_item(
      std::make_unique<ArrowMenuItem>(context, ArrowType::Down, [this] {
        if (goals_scroll_ < goals_box_->max_scroll()) {
          goals_scroll_++;
          mark_dirty();
        }
      }));

  // Middle column: Disk toggles the left box, OK confirms
  add_menu_item(
      std::make_unique<PositionMarkerMenuItem>(context, BUTTONS_X, BUTTONS_Y));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Disk", [this] { toggle_disk(); }, face, style, sel_style,
      item_align));
  add_menu_item(std::make_unique<SpacerMenuItem>(context, 14));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "OK", [this] { confirm(); }, face, style, sel_style,
      item_align));
  add_menu_item(
      std::make_unique<SpacerMenuItem>(context, BACK_BUTTON_Y_OFFSET));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Back", [this] { state_.ui().pop(); }, FontFace::RUBY, style,
      sel_style, item_align));

  // Right column: the eight named built-in missions. The arrows are there
  // because the original has them but they don't do anything.
  add_menu_item(
      std::make_unique<PositionMarkerMenuItem>(context, NAMES_BOX_X, 0));
  add_menu_item(std::make_unique<ArrowMenuItem>(context, ArrowType::Up, [] {}));

  add_menu_item(std::make_unique<MissionTypeBox>(
      context, Size{NAMES_BOX_W, GOALS_BOX_H},
      std::vector<std::string>{NAMED_MISSIONS.begin(), NAMED_MISSIONS.end()},
      [this](int index) { select_named_mission(index); }));

  add_menu_item(
      std::make_unique<ArrowMenuItem>(context, ArrowType::Down, [] {}));

  show_goals();
}

void MenuMissionSelect::select_named_mission(int index) {
  auto path = fileutils::find_file_path(compat::format("m/{}.mis", index));
  if (!path) {
    context_.popup->create<AlertPopup>(
        compat::format("Mission file m/{}.mis not found.", index));
    return;
  }
  load_mission(*path);
}

void MenuMissionSelect::select_player_mission(int index) {
  if (index < 0 || index >= static_cast<int>(player_files_.size())) {
    return;
  }
  load_mission(std::filesystem::path(platform_paths::missions_dir()) /
               player_files_[index].filename);
}

void MenuMissionSelect::load_mission(const std::filesystem::path& path) {
  auto target = core::mission::read_mission_file(path);
  if (!target) {
    context_.popup->create<AlertPopup>("Not a valid mission file.");
    return;
  }
  if (state_.rules().fix_legacy_bugs) {
    // The original's mission files date from the German 1994 release, so it's
    // not possible to get 100% because the missions are in 1994 but we start in
    // 1995. Let's fix this:
    core::mission::shift_mission_to_game_start(*target);
  }
  selected_ = std::move(*target);
  show_goals();
}

void MenuMissionSelect::toggle_disk() {
  if (goals_mode_ == GoalsBoxMode::Files) {
    show_goals();
  }
  else {
    show_files();
  }
}

void MenuMissionSelect::show_goals() {
  goals_mode_ = GoalsBoxMode::Goals;
  goals_scroll_ = 0;

  std::vector<std::string> lines;
  if (selected_) {
    for (const auto& milestone : selected_->milestones) {
      lines.push_back(milestone.date.cv_date_string());
      const std::string text =
          state_.career_log().cv_label(milestone.message_id);
      for (std::string_view line : Font::split_by_pipe(text)) {
        lines.emplace_back(line);
      }
      lines.emplace_back();
    }
  }
  goals_box_->set_lines(std::move(lines), false, "Select mission...");
  mark_dirty();
}

void MenuMissionSelect::show_files() {
  goals_mode_ = GoalsBoxMode::Files;
  goals_scroll_ = 0;
  player_files_ = core::mission::list_player_mission_files();

  std::vector<std::string> lines;
  lines.reserve(player_files_.size());
  for (const auto& file : player_files_) {
    std::string name = file.filename;
    if (name.ends_with(".mis")) {
      name = name.substr(0, name.size() - 4);
    }
    lines.push_back(std::move(name));
  }
  goals_box_->set_lines(std::move(lines), true, "No missions available");
  mark_dirty();
}

void MenuMissionSelect::confirm() {
  // OK without a loaded mission leaves the screen just like in the original
  // (even though we added a back button as well)
  if (!selected_) {
    state_.ui().pop();
    return;
  }

  Logger::info("Starting mission: {} ({} milestones)", selected_->source_name,
               selected_->milestones.size());
  state_.initializer().mission = std::move(selected_);
  state_.ui().push<ScreenTag>(ScreenTag::Kind::DifficultySelection);
}

Surface MenuMissionSelect::background() const {
  auto background = default_background("Select Mission");

  // Gray shadow boxes for both list columns (same as MenuGameLoad)
  const std::array<std::pair<int, int>, 2> boxes = {
      std::pair{GOALS_BOX_OFFSET_X, GOALS_BOX_W},
      std::pair{GOALS_BOX_OFFSET_X + NAMES_BOX_X, NAMES_BOX_W},
  };
  for (const auto& [x, w] : boxes) {
    ViewRect shadow_box(x - 2, GOALS_BOX_OFFSET_Y + 4, w,
                        GOALS_BOX_H + (BOX_ARROW_H * 2) + 2);
    background.fill_rect(shadow_box, 14);

    ViewRect box(x, GOALS_BOX_OFFSET_Y, w + 2, GOALS_BOX_H + (BOX_ARROW_H * 2));
    background.fill_rect(box, 13);
  }

  return background;
}
