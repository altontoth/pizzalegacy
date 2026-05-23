/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/MenuOptions.h"
#include "core/state/GameState.h"
#include "core/util/ConfigManager.h"
#include "engine/DisplayConstants.h"
#include "views/ui/widgets/EnumMenuItem.h"
#include "views/ui/widgets/SpacerMenuItem.h"
#include "views/ui/widgets/TextMenuItem.h"
#include "views/ui/widgets/ToggleMenuItem.h"

MenuOptions::MenuOptions(GameContext& context, GameState& state)
    : Menu(context, state, ViewPoint{120, 140}, 3) {

  const FontFace face = FontFace::PIZ_NUMB;
  const TextStyle style{
      .color = 240,
  };
  const TextStyle sel_style{
      .color = 29,
  };

  add_menu_item(std::make_unique<ToggleMenuItem>(
      context, "Map FX", state.options().map_fx, [&state, this] {
        state.options().map_fx = !state.options().map_fx;
        mark_dirty();
      }));
  add_menu_item(std::make_unique<ToggleMenuItem>(
      context, "Restaurant FX", state.options().restaurant_fx, [&state, this] {
        state.options().restaurant_fx = !state.options().restaurant_fx;
        mark_dirty();
      }));
  add_menu_item(std::make_unique<ToggleMenuItem>(
      context, "Music", state.options().music, [&state, this] {
        state.options().music = !state.options().music;
        mark_dirty();
      }));
  add_menu_item(std::make_unique<ToggleMenuItem>(
      context, "Automatic Report", state.options().automatic_report,
      [&state, this] {
        state.options().automatic_report = !state.options().automatic_report;
        mark_dirty();
      }));
  add_menu_item(std::make_unique<ToggleMenuItem>(
      context, "CRT Aspect Ratio (4:3)", state.options().crt_aspect_ratio,
      [&state, this] {
        state.options().crt_aspect_ratio = !state.options().crt_aspect_ratio;
        state.request_crt_change(state.options().crt_aspect_ratio);
        mark_dirty();
      }));
  add_menu_item(std::make_unique<EnumMenuItem<ScreenResolution>>(
      context, "Screen Resolution", state.options().resolution,
      std::vector{ScreenResolution::Res_640x400,
                  ScreenResolution::Res_1280x800},
      [&state, this] {
        state.request_resolution_change(state.options().resolution);
        mark_dirty();
      },
      face));
  add_menu_item(std::make_unique<ToggleMenuItem>(
      context, "Fix original bugs", state.options().fix_legacy_bugs,
      [&state, this] {
        state.options().fix_legacy_bugs = !state.options().fix_legacy_bugs;
        mark_dirty();
      }));
  add_menu_item(std::make_unique<ToggleMenuItem>(
      context, "Quality of Life improvements", state.options().qol_improvements,
      [&state, this] {
        state.options().qol_improvements = !state.options().qol_improvements;
        mark_dirty();
      }));
  add_menu_item(std::make_unique<ToggleMenuItem>(
      context, "Invert map scrolling", state.options().rmb_drag_inverted,
      [&state, this] {
        state.options().rmb_drag_inverted = !state.options().rmb_drag_inverted;
        mark_dirty();
      }));
  add_menu_item(std::make_unique<ToggleMenuItem>(
      context, "Invert scroll wheel direction",
      state.options().scroll_zoom_inverted, [&state, this] {
        state.options().scroll_zoom_inverted =
            !state.options().scroll_zoom_inverted;
        mark_dirty();
      }));

  add_menu_item(std::make_unique<ToggleMenuItem>(
      context, "Auto save", state.options().auto_save, [&state, this] {
        state.options().auto_save = !state.options().auto_save;
        mark_dirty();
      }));

  add_menu_item(std::make_unique<SpacerMenuItem>(context, 30));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Save and exit",
      [&context, &state] {
        context.config->save_options(state.options());
        state.ui().pop();
      },
      face, style, sel_style));

  update_selection(nullptr);
}

Surface MenuOptions::background() const {
  return default_background("Options");
}
