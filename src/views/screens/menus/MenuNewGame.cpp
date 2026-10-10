/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/MenuNewGame.h"

#include <cassert>
#include "core/state/GameState.h"
#include "core/tutorial/TutorialGame.h"
#include "views/screens/menus/MenuTheme.h"
#include "views/ui/widgets/PositionMarkerMenuItem.h"
#include "views/ui/widgets/SpacerMenuItem.h"
#include "views/ui/widgets/TextMenuItem.h"

using namespace views::ui::menu;

#define DIFF_INIT_POS_X 0
#define DIFF_INIT_POS_Y 130
#define DIFF_POS_OFFSET 40

MenuNewGame::MenuNewGame(GameContext& context, GameState& state)
    : Menu(context, state, ViewPoint{DIFF_INIT_POS_X, DIFF_INIT_POS_Y}, 0) {

  constexpr int space_between_lines = 14;

  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Freemode",
      [&state] {
        Logger::debug("Set game mode to Freemode");
        state.begin_initialization();
        state.initializer().mode = GameMode::FREEMODE;
        state.ui().push<ScreenTag>(ScreenTag::Kind::DifficultySelection);
      },
      face, style, sel_style, item_align));
  add_menu_item(std::make_unique<SpacerMenuItem>(context, space_between_lines));

  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Mission Mode",
      [&state] {
        Logger::debug("Set game mode to Mission Mode");
        state.begin_initialization();
        state.initializer().mode = GameMode::MISSION_MODE;
        state.ui().push<ScreenTag>(ScreenTag::Kind::MissionSelect);
      },
      face, style, sel_style, item_align));
  add_menu_item(std::make_unique<SpacerMenuItem>(context, space_between_lines));

  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Quickstart",
      [&state] {
        Logger::debug("Set game mode to Quickstart");
        state.begin_initialization();
        state.initializer().mode = GameMode::QUICKSTART;
        state.ui().push<ScreenTag>(ScreenTag::Kind::MenuQuickstart);
      },
      face, style, sel_style, item_align));
  add_menu_item(std::make_unique<SpacerMenuItem>(context, space_between_lines));

  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Tutorial",
      [this] {
        Logger::debug("Starting the tutorial");
        core::tutorial::start_tutorial_game(state_, context_);
      },
      face, style, sel_style, item_align));

  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 270, 244));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Back", [this] { state_.ui().pop(); }, FontFace::RUBY, style,
      sel_style, item_align));
}

Surface MenuNewGame::background() const {
  return default_background("New Game");
}
