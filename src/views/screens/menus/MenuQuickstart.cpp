/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/MenuQuickstart.h"

#include <cassert>
#include "core/init/NewGameConfig.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "views/screens/menus/MenuTheme.h"
#include "views/ui/widgets/SpacerMenuItem.h"
#include "views/ui/widgets/TextMenuItem.h"

using namespace views::ui::menu;

#define DIFF_INIT_POS_X 0
#define DIFF_INIT_POS_Y 130
#define DIFF_POS_OFFSET 40

MenuQuickstart::MenuQuickstart(GameContext& context, GameState& state)
    : Menu(context, state, ViewPoint{DIFF_INIT_POS_X, DIFF_INIT_POS_Y}, 0) {

  const std::pair<std::string, init::RestaurantSize> modes[] = {
      {"Large restaurant", init::RestaurantSize::BIG},
      {"Small restaurant", init::RestaurantSize::SMALL},
  };

  constexpr int space_between_lines = 14;

  for (const auto& mode : modes) {
    add_menu_item(std::make_unique<TextMenuItem>(
        context, mode.first,
        [mode, &state] {
          Logger::debug("Quickstarting with a {}", mode.first.c_str());

          state.initializer().size = mode.second;
          state.ui().push<ScreenTag>(ScreenTag::Kind::DifficultySelection);
        },
        face, style, sel_style));
    add_menu_item(
        std::make_unique<SpacerMenuItem>(context, space_between_lines));
  }
}

Surface MenuQuickstart::background() const {
  return default_background("Quickstart");
}
