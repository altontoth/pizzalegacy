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
  // * indicates requires restart to take effect (but we don't tell anyone that)
  add_menu_item(std::make_unique<ToggleMenuItem>(
      context, "CRT Aspect Ratio (4:3) *", state.options().crt_aspect_ratio,
      [&state, this] {
        state.options().crt_aspect_ratio = !state.options().crt_aspect_ratio;
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

  /*
  // Asset format toggle - conditional on availability
  auto availability = context.config->detect_available_formats();

  if (availability.dos_available && availability.amiga_available) {
    // Both formats available - show interactive toggle
    add_menu_item(std::make_unique<ToggleMenuItem>(
        context,
        "Asset Format *", // * = requires restart
        format_is_dos_,
        "AMIGA", // false_label
        "DOS",   // true_label
        [this] {
          // Toggle the local state first
          format_is_dos_ = !format_is_dos_;
          // Then save the new format to config
          auto new_format = format_is_dos_ ? "dos" : "amiga";
          context_.config->value("active_asset_format", new_format);
          context_.config->save_config();
          mark_dirty();
        },
        face));
  }
  else {
    // Only one available - show non-interactive text
    std::string format_name = availability.dos_available ? "DOS" : "AMIGA";
    std::string label = "Asset Format: " + format_name;

    add_menu_item(std::make_unique<TextMenuItem>(
        context, label, []() {}, // Empty action - non-interactive
        face, style, sel_style));
  }
  */

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
