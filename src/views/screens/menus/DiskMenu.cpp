/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/DiskMenu.h"
#include "core/state/GameState.h"
#include "views/screens/menus/MenuTheme.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/ui/widgets/TextMenuItem.h"

using namespace views::ui::menu;

DiskMenu::DiskMenu(GameContext& context, GameState& state)
    : Menu(context, state, ViewPoint{0, 160}, 10) {
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Save",
      [&state] { state.ui().push<ScreenTag>(ScreenTag::Kind::MenuSave); }, face,
      style, sel_style, item_align));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Old Game",
      [&state] { state.ui().push<ScreenTag>(ScreenTag::Kind::MenuGameLoad); },
      face, style, sel_style, item_align));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Options",
      [&state] { state.ui().push<ScreenTag>(ScreenTag::Kind::MenuOptions); },
      face, style, sel_style, item_align));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Exit",
      [&state, &context] {
        context.popup->origin(ViewPoint{26, 40});
        context.popup->create<ConfirmPopup>("Are you sure?",
                                            [&state](bool sure) {
                                              if (sure) {
                                                state.request_exit();
                                              }
                                            });
      },
      face, style, sel_style, item_align));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "OK", [&state] { state.ui().pop(); }, face, style, sel_style,
      item_align));

  // Make sure nothing is selected/higlighted by default
  update_selection(nullptr);
}

Surface DiskMenu::background() const { return default_background("Disk Menu"); }
