/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/DifficultySelection.h"

#include <cassert>
#include "core/init/NewGameConfig.h"
#include "core/state/GameState.h"
#include "views/screens/menus/MenuTheme.h"
#include "views/ui/widgets/SpacerMenuItem.h"
#include "views/ui/widgets/TextMenuItem.h"

using namespace views::ui::menu;

#define DIFF_INIT_POS_X 0
#define DIFF_INIT_POS_Y 130
#define DIFF_POS_OFFSET 40

DifficultySelection::DifficultySelection(GameContext& context, GameState& state)
    : Menu(context, state, ViewPoint{DIFF_INIT_POS_X, DIFF_INIT_POS_Y}, 0) {

  const std::pair<std::string, init::DifficultyLevel> options[] = {
      {"pushover", init::DifficultyLevel::PUSHOVER},
      {"easy", init::DifficultyLevel::EASY},
      {"normal", init::DifficultyLevel::NORMAL},
      {"difficult", init::DifficultyLevel::DIFFICULT},
      {"impossible", init::DifficultyLevel::IMPOSSIBLE}};

  constexpr int space_between_lines = 14;

  for (const auto& option : options) {
    add_menu_item(std::make_unique<TextMenuItem>(
        context, option.first,
        [option, &state] {
          Logger::debug("Set difficulty to {}", option.first.c_str());

          state.initializer().difficulty = option.second;
          state.ui().push<ScreenTag>(ScreenTag::Kind::CharacterSelection);
        },
        face, style, sel_style));
    add_menu_item(
        std::make_unique<SpacerMenuItem>(context, space_between_lines));
  }
}

Surface DifficultySelection::background() const {
  return default_background("Select level");
}
