/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/MenuBindings.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "core/state/GameState.h"
#include "engine/PlatformPaths.h"
#include "engine/input/Action.h"
#include "engine/input/BindingsFile.h"
#include "engine/input/Key.h"
#include "engine/ui/TextAnchor.h"
#include "resources/FontManager.h"
#include "views/ui/widgets/BindingMenuItem.h"
#include "views/ui/widgets/LabelMenuItem.h"
#include "views/ui/widgets/PositionMarkerMenuItem.h"
#include "views/ui/widgets/SpacerMenuItem.h"
#include "views/ui/widgets/TextMenuItem.h"

namespace {
using input::Action;

const TextStyle style{.color = 240};
const TextStyle sel_style{.color = 29};
const TextStyle heading_style{.color = 26};
// Above every heading but the first.
constexpr int heading_gap = 6;

// The actions the screen offers; the rest can only be bound in the file.
struct ActionText {
  Action action;
  std::string_view label;
  std::string_view help;
};
constexpr ActionText action_texts[] = {
    {Action::TakeScreenshot, "Take screenshot",
     "Save the screen to the screenshots folder."},
    {Action::ToggleFullscreen, "Fullscreen",
     "Switch between a window and the full screen."},
    {Action::TogglePause, "Pause", "Pause the game."},
    {Action::TimeScale1, "Normal speed", "Run the game at normal speed."},
    {Action::TimeScale2, "Speed x2", "Run the game twice as fast."},
    {Action::TimeScale3, "Speed x3", "Run the game three times as fast."},
    {Action::TimeScale5, "Speed x5", "Run the game five times as fast."},
    {Action::TimeScale10, "Speed x10", "Run the game ten times as fast."},
    {Action::Fire, "Select or fire",
     "Choose the selected menu item, and shoot in a burglary."},
    {Action::MoveUp, "Up",
     "Move the menu selection, change a number in a popup, recall console "
     "history, and move in a burglary."},
    {Action::MoveDown, "Down",
     "Move the menu selection, change a number in a popup, recall console "
     "history, and move in a burglary."},
    {Action::MoveLeft, "Left",
     "Change the selected menu setting, and move in a burglary."},
    {Action::MoveRight, "Right",
     "Change the selected menu setting, and move in a burglary."},
    {Action::P2_MoveUp, "Player two up",
     "Move the defender in a burglary with a second player."},
    {Action::P2_MoveDown, "Player two down",
     "Move the defender in a burglary with a second player."},
    {Action::P2_MoveLeft, "Player two left",
     "Move the defender in a burglary with a second player."},
    {Action::P2_MoveRight, "Player two right",
     "Move the defender in a burglary with a second player."},
    {Action::P2_Fire, "Player two fire",
     "Shoot as the defender in a burglary with a second player."},
    {Action::ScrollUp, "Scroll map up", "Scroll the map."},
    {Action::ScrollDown, "Scroll map down", "Scroll the map."},
    {Action::ScrollLeft, "Scroll map left", "Scroll the map."},
    {Action::ScrollRight, "Scroll map right", "Scroll the map."},
    {Action::LoadRecipe, "Import recipe",
     "Load a recipe from the recipes folder on the pizza design screen."},
    {Action::SaveRecipe, "Export recipe",
     "Save the pizza on the pizza design screen to the recipes folder."},
    {Action::NextRestaurant, "Next restaurant",
     "On the city map, move to and select your next restaurant; in the "
     "restaurant view, go to it."},
    {Action::PreviousRestaurant, "Previous restaurant",
     "On the city map, move to and select your previous restaurant; in the "
     "restaurant view, go to it."},
    {Action::ToggleRestaurantView, "Toggle kitchen view",
     "Switch the restaurant view to the kitchen and back."},
};

const ActionText* text_for(Action a) {
  for (const ActionText& text : action_texts) {
    if (text.action == a) {
      return &text;
    }
  }
  return nullptr;
}

// Bound to an action that players can't reassign.
bool is_fixed_chord(const input::InputManager& in, input::Binding chord) {
  return std::ranges::any_of(in.button_bindings(), [&](const auto& entry) {
    return entry.first == chord && in.is_fixed(entry.second);
  });
}
} // namespace

MenuBindings::MenuBindings(GameContext& context, GameState& state)
    : Menu(context, state, ViewPoint{120, 140}, 3) {
  const input::InputManager& in = *context_.input;
  const auto& actions = in.documented_actions();
  const auto& sections = in.doc_sections();

  for (std::size_t s = 0; s < sections.size(); ++s) {
    const std::size_t end =
        s + 1 < sections.size() ? sections[s + 1].first_action : actions.size();
    bool heading_added = false;
    for (std::size_t i = sections[s].first_action; i < end; ++i) {
      const ActionText* text = text_for(actions[i]);
      if (!text) {
        continue;
      }
      if (!heading_added) {
        if (!rows_.empty()) {
          add_menu_item(
              std::make_unique<SpacerMenuItem>(context_, heading_gap));
        }
        add_menu_item(std::make_unique<LabelMenuItem>(
            context_, sections[s].label, FontFace::PIZ_NUMB, heading_style));
        heading_added = true;
      }
      auto row = std::make_unique<BindingMenuItem>(
          context_, std::string(text->label), text->action,
          [this](BindingMenuItem& r) { choose(r); });
      row->help_text(std::string(text->help));
      rows_.push_back(row.get());
      add_menu_item(std::move(row));
    }
  }

  // Leaves room below for the help text.
  constexpr int scroll_height = 170;
  end_scrolling_items(scroll_height);

  // The bottom left corner, level with Back
  constexpr int restore_y = 238;
  add_menu_item(
      std::make_unique<PositionMarkerMenuItem>(context_, 0, restore_y));
  add_menu_item(std::make_unique<TextMenuItem>(
                    context_, "Restore defaults",
                    [this] { restore_defaults(); }, FontFace::PIZ_NUMB, style,
                    sel_style))
      .help_text("Put back the default keys for every action on this screen.");

  // The bottom right corner, where the other menus have theirs
  constexpr int back_x = 426;
  constexpr int back_y = 230;
  add_menu_item(
      std::make_unique<PositionMarkerMenuItem>(context_, back_x, back_y));
  add_menu_item(std::make_unique<TextMenuItem>(
      context_, "Back", [&state] { state.ui().pop(); }, FontFace::RUBY, style,
      sel_style));

  update_selection(nullptr);
}

MenuBindings::~MenuBindings() {
  if (waiting_) {
    context_.input->cancel_capture();
  }
}

bool MenuBindings::on_action(const input::ActionEvent& ev) {
  if (ev.state.pressed && ev.action == Action::Backspace) {
    auto it = std::ranges::find(rows_, selected_option_);
    if (it != rows_.end()) {
      clear(**it);
      return true;
    }
  }
  return Menu::on_action(ev);
}

void MenuBindings::choose(BindingMenuItem& row) {
  stop_waiting();
  waiting_ = &row;
  row.waiting(true);
  context_.input->begin_capture(
      [this](std::optional<input::CapturedInput> captured) {
        on_captured(std::move(captured));
      });
  mark_dirty();
}

void MenuBindings::on_captured(std::optional<input::CapturedInput> captured) {
  BindingMenuItem& row = *waiting_;
  input::InputManager& in = *context_.input;

  if (const auto* chord =
          captured ? std::get_if<input::Binding>(&*captured) : nullptr) {
    if (is_fixed_chord(in, *chord)) {
      choose(row);
      return;
    }
    in.remove_button_bindings(row.action());
    in.bind_button(*chord, row.action());
  }
  else if (const auto* button =
               captured ? std::get_if<input::GamepadButton>(&*captured)
                        : nullptr) {
    in.remove_gamepad_bindings(row.action());
    in.bind_gamepad_button(*button, row.action());
  }

  row.waiting(false);
  waiting_ = nullptr;
  if (captured) {
    save();
  }
  mark_dirty();
}

void MenuBindings::clear(BindingMenuItem& row) {
  stop_waiting();
  context_.input->remove_bindings(row.action());
  save();
  mark_dirty();
}

void MenuBindings::restore_defaults() {
  stop_waiting();
  std::vector<Action> actions;
  for (const BindingMenuItem* row : rows_) {
    actions.push_back(row->action());
  }
  input::restore_default_bindings(*context_.input, actions);
  save();
  mark_dirty();
}

void MenuBindings::stop_waiting() {
  if (!waiting_) {
    return;
  }
  context_.input->cancel_capture();
  waiting_->waiting(false);
  waiting_ = nullptr;
}

void MenuBindings::save() {
  input::save_bindings_file(*context_.input,
                            platform_paths::bindings_file_path());
}

Surface MenuBindings::background() const {
  Surface background = default_background("Key bindings");
  constexpr ViewPoint hint_pos{620, 118};
  context_.font->render_text_onto(
      background, "Enter to change, Backspace to clear", hint_pos,
      FontFace::SUPERSMA, 240, TextAnchor::End);
  return background;
}
