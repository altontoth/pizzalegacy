/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/MenuSave.h"

#include <cassert>
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "views/screens/menus/MenuTheme.h"
#include "views/ui/widgets/SpacerMenuItem.h"
#include "views/ui/widgets/TextMenuItem.h"

using namespace views::ui::menu;

#define DIFF_INIT_POS_X 0
#define DIFF_INIT_POS_Y 130
#define DIFF_POS_OFFSET 40

MenuSave::MenuSave(GameContext& context, GameState& state)
    : Menu(context, state, ViewPoint{DIFF_INIT_POS_X, DIFF_INIT_POS_Y}, 0) {

  constexpr int space_between_lines = 14;

  add_menu_item(std::make_unique<TextMenuItem>(
      context, "as game",
      [&state] { state.ui().push<ScreenTag>(ScreenTag::Kind::MenuSaveAsGame); },
      face, style, sel_style, item_align));

  add_menu_item(std::make_unique<SpacerMenuItem>(context, space_between_lines));

  add_menu_item(std::make_unique<TextMenuItem>(
      context, "as mission",
      [&state] {
        state.ui().push<ScreenTag>(ScreenTag::Kind::MenuSaveAsMission);
      },
      face, style, sel_style, item_align));
}

Surface MenuSave::background() const { return default_background("Save"); }
