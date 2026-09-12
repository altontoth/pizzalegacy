/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/MenuOptions.h"

#include <string>
#include <vector>

#include "core/state/GameOptions.h"
#include "core/state/GameState.h"
#include "core/util/ConfigManager.h"
#include "engine/DisplayConstants.h"
#include "engine/audio/AudioManager.h"
#include "engine/input/Action.h"
#include "engine/input/InputManager.h"
#include "engine/input/Key.h"
#include "views/ui/widgets/EnumMenuItem.h"
#include "views/ui/widgets/IntSliderMenuItem.h"
#include "views/ui/widgets/SliderMenuItem.h"
#include "views/ui/widgets/SpacerMenuItem.h"
#include "views/ui/widgets/TextMenuItem.h"
#include "views/ui/widgets/ToggleMenuItem.h"

namespace {
constexpr FontFace face = FontFace::PIZ_NUMB;
const TextStyle style{
    .color = 240,
};
const TextStyle sel_style{
    .color = 29,
};

// "Ctrl+F or F11", or empty when nothing is bound.
std::string chord_list(const std::vector<input::Binding>& chords) {
  std::string joined;
  for (const input::Binding& chord : chords) {
    if (!joined.empty()) {
      joined += " or ";
    }
    joined += input::chord_to_string(chord);
  }
  return joined;
}
} // anonymous namespace

MenuOptions::MenuOptions(GameContext& context, GameState& state)
    : Menu(context, state, ViewPoint{120, 140}, 12), title_("Options") {
  build_chooser(state);
  update_selection(nullptr);
}

MenuOptions::MenuOptions(GameContext& context, GameState& state,
                         OptionsScreen screen)
    : Menu(context, state, ViewPoint{120, 140}, 3) {
  switch (screen) {
  case OptionsScreen::Audio:
    title_ = "Options - Audio";
    build_audio(state);
    break;
  case OptionsScreen::Graphics:
    title_ = "Options - Graphics";
    build_graphics(state);
    break;
  case OptionsScreen::Controls:
    title_ = "Options - Controls";
    build_controls(state);
    break;
  case OptionsScreen::Gameplay:
    title_ = "Options - Gameplay";
    build_gameplay(state);
    break;
  }
  add_save_and_back(state);
  update_selection(nullptr);
}

void MenuOptions::build_chooser(GameState& state) {
  add_menu_item(std::make_unique<TextMenuItem>(
      context_, "> Audio",
      [&state] { state.ui().push<OptionsMenuState>(OptionsScreen::Audio); },
      face, style, sel_style));
  add_menu_item(std::make_unique<TextMenuItem>(
      context_, "> Graphics",
      [&state] { state.ui().push<OptionsMenuState>(OptionsScreen::Graphics); },
      face, style, sel_style));
  add_menu_item(std::make_unique<TextMenuItem>(
      context_, "> Controls",
      [&state] { state.ui().push<OptionsMenuState>(OptionsScreen::Controls); },
      face, style, sel_style));
  add_menu_item(std::make_unique<TextMenuItem>(
      context_, "> Gameplay",
      [&state] { state.ui().push<OptionsMenuState>(OptionsScreen::Gameplay); },
      face, style, sel_style));

  // Sub-screens save their own changes when leaving, so from the main screen we
  // can just pop.
  add_menu_item(std::make_unique<SpacerMenuItem>(context_, 30));
  add_menu_item(std::make_unique<TextMenuItem>(
      context_, "Back", [&state] { state.ui().pop(); }, face, style,
      sel_style));
}

void MenuOptions::build_audio(GameState& state) {
  add_menu_item(std::make_unique<ToggleMenuItem>(
                    context_, "Map FX", state.options().map_fx,
                    [&state, this] {
                      state.options().map_fx = !state.options().map_fx;
                      mark_dirty();
                    }))
      .help_text("Background sounds on the city map.");
  add_menu_item(std::make_unique<ToggleMenuItem>(
                    context_, "Restaurant FX", state.options().restaurant_fx,
                    [&state, this] {
                      state.options().restaurant_fx =
                          !state.options().restaurant_fx;
                      mark_dirty();
                    }))
      .help_text("Background sounds inside a restaurant.");
  add_menu_item(std::make_unique<ToggleMenuItem>(
      context_, "Music", state.options().music, [&state, this] {
        state.options().music = !state.options().music;
        context_.audio->music_enabled(state.options().music);
        mark_dirty();
      }));
  add_menu_item(std::make_unique<SliderMenuItem>(
      context_, "Sound Volume", state.options().sound_volume,
      [this](float v) { context_.audio->sound_volume(v); }));
  add_menu_item(std::make_unique<SliderMenuItem>(
      context_, "Music Volume", state.options().music_volume,
      [this](float v) { context_.audio->music_volume(v); }));
}

void MenuOptions::build_graphics(GameState& state) {
  add_menu_item(
      std::make_unique<ToggleMenuItem>(
          context_, "CRT Aspect Ratio (4:3)", state.options().crt_aspect_ratio,
          [&state, this] {
            state.options().crt_aspect_ratio =
                !state.options().crt_aspect_ratio;
            state.request_crt_change(state.options().crt_aspect_ratio);
            mark_dirty();
          }))
      .help_text("The game draws 640x400, and a nineties monitor stretched "
                 "that to 4:3; this does the same.");
  add_menu_item(
      std::make_unique<ToggleMenuItem>(
          context_, "Double Size", state.options().double_size,
          [&state, this] {
            state.options().double_size = !state.options().double_size;
            state.request_double_size_change(state.options().double_size);
            mark_dirty();
          }))
      .help_text("Show the window at twice the size. You can also just resize "
                 "it by grabbing a corner.");
  MenuItem& fullscreen = add_menu_item(std::make_unique<ToggleMenuItem>(
      context_, "Fullscreen", state.options().fullscreen, [&state, this] {
        state.options().fullscreen = !state.options().fullscreen;
        state.request_fullscreen_change(state.options().fullscreen);
        mark_dirty();
      }));
  if (const std::string keys = chord_list(
          context_.input->button_bindings_for(input::Action::ToggleFullscreen));
      !keys.empty()) {
    fullscreen.help_text("You can also toggle it any time with " + keys + ".");
  }
  add_menu_item(
      std::make_unique<EnumMenuItem<FpsOption>>(
          context_, "Render FPS", state.options().render_fps,
          std::vector{FpsOption::Fps30, FpsOption::Fps60, FpsOption::Fps120},
          [this] { mark_dirty(); }, face))
      .help_text("How often the screen is redrawn. The game itself animates "
                 "30 times a second at normal speed, so a higher number "
                 "mainly smooths the cursor.");
  add_menu_item(std::make_unique<ToggleMenuItem>(
                    context_, "VSync", state.options().vsync,
                    [&state, this] {
                      state.options().vsync = !state.options().vsync;
                      mark_dirty();
                    }))
      .help_text("Synchronize drawing with the screen's refresh rate to prevent"
                 " tearing.");
}

void MenuOptions::build_gameplay(GameState& state) {
  add_menu_item(
      std::make_unique<ToggleMenuItem>(context_, "Automatic Report",
                                       state.options().automatic_report,
                                       [&state, this] {
                                         state.options().automatic_report =
                                             !state.options().automatic_report;
                                         mark_dirty();
                                       }))
      .help_text("Automatically show the daily turnover report at the end of "
                 "the turn.");
  add_menu_item(
      std::make_unique<ToggleMenuItem>(
          context_, "Fix original bugs", state.options().fix_legacy_bugs,
          [&state, this] {
            state.options().fix_legacy_bugs = !state.options().fix_legacy_bugs;
            mark_dirty();
          }))
      .help_text("Fix various bugs from the original; see "
                 "docs/MODERNIZATIONS.md for details.");
  add_menu_item(
      std::make_unique<ToggleMenuItem>(context_, "Quality of Life improvements",
                                       state.options().qol_improvements,
                                       [&state, this] {
                                         state.options().qol_improvements =
                                             !state.options().qol_improvements;
                                         mark_dirty();
                                       }))
      .help_text("Enable various modernizations of the UI and mouse behavior.");
  add_menu_item(
      std::make_unique<EnumMenuItem<AutosaveFrequency>>(
          context_, "Auto save", state.options().autosave_frequency,
          std::vector{AutosaveFrequency::Off, AutosaveFrequency::Daily,
                      AutosaveFrequency::Weekly, AutosaveFrequency::Monthly},
          [this] { mark_dirty(); }, face))
      .help_text("Automatically save the game at the end of the day, week or "
                 "month.");
  add_menu_item(std::make_unique<IntSliderMenuItem>(
      context_, "Auto save slots", state.options().autosave_count,
      GameOptions::AUTOSAVE_COUNT_MIN, GameOptions::AUTOSAVE_COUNT_MAX,
      [this](int) { mark_dirty(); }));
}

void MenuOptions::build_controls(GameState& state) {
  add_menu_item(
      std::make_unique<ToggleMenuItem>(context_, "Invert map scrolling",
                                       state.options().rmb_drag_inverted,
                                       [&state, this] {
                                         state.options().rmb_drag_inverted =
                                             !state.options().rmb_drag_inverted;
                                         mark_dirty();
                                       }))
      .help_text("Which way the map moves when you drag it with the right "
                 "mouse button.");
  add_menu_item(std::make_unique<ToggleMenuItem>(
                    context_, "Invert scroll wheel direction",
                    state.options().scroll_wheel_inverted,
                    [&state, this] {
                      state.options().scroll_wheel_inverted =
                          !state.options().scroll_wheel_inverted;
                      mark_dirty();
                    }))
      .help_text("Reverse the direction of the scroll wheel.");
  add_menu_item(
      std::make_unique<ToggleMenuItem>(
          context_, "Use software cursor", state.options().software_cursor,
          [&state, this] {
            state.options().software_cursor = !state.options().software_cursor;
            context_.cursor->software_mode(state.options().software_cursor);
            mark_dirty();
          }))
      .help_text("Draw the cursor with the game so it stays in sync with the "
                 "game.");
}

void MenuOptions::add_save_and_back(GameState& state) {
  add_menu_item(std::make_unique<SpacerMenuItem>(context_, 30));
  // There is no way to leave the options without saving, so "Back" saves the
  // current settings and returns to the main screen.
  add_menu_item(std::make_unique<TextMenuItem>(
      context_, "Back",
      [this, &state] {
        context_.config->save_options(state.options());
        state.ui().pop();
      },
      face, style, sel_style));
}

Surface MenuOptions::background() const { return default_background(title_); }
