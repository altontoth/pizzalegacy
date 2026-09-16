/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/MenuOptions.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "core/state/GameOptions.h"
#include "core/state/GameState.h"
#include "core/util/ConfigManager.h"
#include "engine/DisplayConstants.h"
#include "engine/audio/AudioManager.h"
#include "engine/input/Action.h"
#include "engine/input/InputManager.h"
#include "engine/input/Key.h"
#include "views/ui/widgets/CheckboxMenuItem.h"
#include "views/ui/widgets/EnumMenuItem.h"
#include "views/ui/widgets/IntSliderMenuItem.h"
#include "views/ui/widgets/PositionMarkerMenuItem.h"
#include "views/ui/widgets/SliderMenuItem.h"
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
                         OptionsScreen screen, bool focus_advanced_toggle)
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
  MenuItem* advanced_toggle = add_save_and_back(state, screen);
  update_selection(focus_advanced_toggle ? advanced_toggle : nullptr);
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
  add_back([&state] { state.ui().pop(); });
}

void MenuOptions::add_back(std::function<void()> action) {
  // The bottom right corner, where the other menus have theirs
  constexpr int back_x = 426;
  constexpr int back_y = 230;
  add_menu_item(
      std::make_unique<PositionMarkerMenuItem>(context_, back_x, back_y));
  add_menu_item(std::make_unique<TextMenuItem>(
      context_, "Back", std::move(action), FontFace::RUBY, style, sel_style));
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

  if (!state.options().show_advanced_options) {
    return;
  }
  auto& options = state.options();
  auto toggle = [this](const std::string& label, bool& value) -> MenuItem& {
    return add_menu_item(std::make_unique<ToggleMenuItem>(context_, label,
                                                          value,
                                                          [&value, this] {
                                                            value = !value;
                                                            mark_dirty();
                                                          }))
        .advanced(true);
  };
  toggle("Random shakedown target",
         options.mafia_shakedowns_target_all_restaurants)
      .help_text("On: the mafia can shake down any of your restaurants. Off: "
                 "only your first one, as in the original.");
  toggle("Fulfillment affects popularity",
         options.demand_fulfillment_popularity)
      .help_text("On: popularity reflects unmet orders. "
                 "Off: it always takes the full drop, as in the DOS original.");
  add_menu_item(std::make_unique<EnumMenuItem<WinCondition>>(
                    context_, "Win condition", options.win_condition,
                    std::vector{WinCondition::Paris, WinCondition::AnyCity,
                                WinCondition::AllCities},
                    [this] { mark_dirty(); }, face))
      .advanced(true)
      .help_text("Paris: win with enough market share in Paris. "
                 "Any: win in any city. All: win in every city.");
  add_menu_item(std::make_unique<IntSliderMenuItem>(
                    context_, "Attack scene speed", options.attack_scene_hz,
                    GameOptions::ATTACK_SCENE_HZ_MIN,
                    GameOptions::ATTACK_SCENE_HZ_MAX,
                    [this](int) { mark_dirty(); }, face, 5))
      .advanced(true)
      .help_text("Updates per second in burglaries. The original ran at 70 "
                 "on a fast 486 and 35 or less on slower machines.");
  toggle("Player colors in dropdowns", options.show_player_color_in_dropdown)
      .help_text("On: the Player and Range lists on the city map show each "
                 "player's color.");
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

  if (!state.options().show_advanced_options) {
    return;
  }
  auto& options = state.options();
  auto changed = [this](int) { mark_dirty(); };
  add_menu_item(std::make_unique<IntSliderMenuItem>(
                    context_, "Key repeat delay", options.key_repeat_delay_ms,
                    GameOptions::KEY_REPEAT_DELAY_MIN,
                    GameOptions::KEY_REPEAT_DELAY_MAX, changed, face, 50))
      .advanced(true)
      .help_text("Milliseconds before a key starts repeating when held down.");
  add_menu_item(std::make_unique<IntSliderMenuItem>(
                    context_, "Key repeat interval",
                    options.key_repeat_interval_ms,
                    GameOptions::KEY_REPEAT_INTERVAL_MIN,
                    GameOptions::KEY_REPEAT_INTERVAL_MAX, changed, face, 10))
      .advanced(true)
      .help_text("Milliseconds between repeats while a key is held down.");
  add_menu_item(std::make_unique<IntSliderMenuItem>(
                    context_, "Tooltip delay", options.tooltip_delay_ms,
                    GameOptions::TOOLTIP_DELAY_MIN,
                    GameOptions::TOOLTIP_DELAY_MAX, changed, face, 100))
      .advanced(true)
      .help_text("Milliseconds the pointer rests on something before its "
                 "tooltip shows.");
}

MenuItem* MenuOptions::add_save_and_back(GameState& state,
                                         OptionsScreen screen) {
  // Leaves room below for the help text.
  constexpr int scroll_height = 170;
  end_scrolling_items(scroll_height);

  MenuItem* advanced_toggle = nullptr;
  if (screen == OptionsScreen::Controls || screen == OptionsScreen::Gameplay) {
    // The bottom left corner, level with Back
    constexpr int toggle_y = 238;
    add_menu_item(
        std::make_unique<PositionMarkerMenuItem>(context_, 0, toggle_y));
    advanced_toggle = &add_menu_item(std::make_unique<CheckboxMenuItem>(
        context_, "Show advanced options",
        state.options().show_advanced_options, [&state, screen] {
          bool& show = state.options().show_advanced_options;
          show = !show;
          state.ui().replace_top<OptionsMenuState>(OptionsMenuState{
              .screen = screen, .focus_advanced_toggle = true});
        }));
  }

  // There is no way to leave the options without saving, so "Back" saves the
  // current settings and returns to the main screen.
  add_back([this, &state] {
    context_.config->save_options(state.options());
    state.ui().pop();
  });
  return advanced_toggle;
}

Surface MenuOptions::background() const { return default_background(title_); }
