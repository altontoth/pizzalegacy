/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/MenuMain.h"
#include "core/state/GameState.h"
#include "engine/DisplayConstants.h"
#include "engine/Logger.h"
#include "engine/PlatformPaths.h"
#include "engine/Version.h"
#include "resources/Font.h"
#include "views/screens/menus/MenuTheme.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/AlertPopup.h"
#include "views/ui/widgets/LabelMenuItem.h"
#include "views/ui/widgets/PositionMarkerMenuItem.h"
#include "views/ui/widgets/SpacerMenuItem.h"
#include "views/ui/widgets/TextMenuItem.h"

using namespace views::ui::menu;

MenuMain::MenuMain(GameContext& context, GameState& state)
    : Menu(context, state, ViewPoint{0, 210}, -8) {
  constexpr int space_between_lines = 24;
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Old Game",
      [&state] { state.ui().push<ScreenTag>(ScreenTag::Kind::MenuGameLoad); },
      face, style, sel_style));
  add_menu_item(std::make_unique<SpacerMenuItem>(context, space_between_lines));

  add_menu_item(std::make_unique<TextMenuItem>(
      context, "New Game",
      [&state] { state.ui().push<ScreenTag>(ScreenTag::Kind::MenuNewGame); },
      face, style, sel_style));
  add_menu_item(std::make_unique<SpacerMenuItem>(context, space_between_lines));

  add_menu_item(std::make_unique<TextMenuItem>(
      context, "High Scores",
      [&state] { state.ui().push<ScreenTag>(ScreenTag::Kind::MenuHighScores); },
      face, style, sel_style));

  add_menu_item(std::make_unique<SpacerMenuItem>(context, space_between_lines));

  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Options",
      [&state] { state.ui().push<ScreenTag>(ScreenTag::Kind::MenuOptions); },
      face, style, sel_style));
  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 276, 164));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Exit", [&state] { state.request_exit(); }, FontFace::RUBY,
      style, sel_style));

  // Debug info in small text in the corner:
  ViewPoint debug_info{10, 170};
  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, debug_info.x,
                                                         debug_info.y));
  TextStyle ts{
      .color = 11,
  };
  {
    std::string version_str =
        std::string("Pizza Legacy version ") + game_meta::VERSION;
#ifndef NDEBUG
    version_str += " (DEBUG)";
#endif
    add_menu_item(std::make_unique<LabelMenuItem>(
        context, std::move(version_str), FontFace::MINI, ts));
  }
  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, debug_info.x,
                                                         debug_info.y + 10));
  ts.color = 13;
  const std::string& base = platform_paths::base_dir();

  add_menu_item(std::make_unique<LabelMenuItem>(
      context, "User directory: " + base, FontFace::MINI, ts));
}

Surface MenuMain::background() const {
  return context_.vga->get("gfx/menu").clone();
}
