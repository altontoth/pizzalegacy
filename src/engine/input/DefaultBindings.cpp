/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cassert>
#include <vector>
#include "engine/Fatal.h"
#include "engine/FormatCompat.h"
#include "engine/input/Action.h"
#include "engine/input/Gamepad.h"
#include "engine/input/InputManager.h"
#include "engine/input/Key.h"

namespace input {

static inline Binding chord(std::string_view s) {
  auto b = parse_chord(s);
  if (!b) {
    fatal(compat::format("Failed to parse binding '{}'", s));
  }
  return b.value();
}

void configure_default_bindings(InputManager& in) {
  in.clear_bindings();

  in.begin_section("Global");
  in.bind_button(chord("Ctrl+S"), Action::TakeScreenshot);
  in.bind_button(chord("Ctrl+F"), Action::ToggleFullscreen);
  in.bind_button(chord("Ctrl+P"), Action::TogglePause);
  in.bind_button(chord("Ctrl+1"), Action::TimeScale1);
  in.bind_button(chord("Ctrl+2"), Action::TimeScale2);
  in.bind_button(chord("Ctrl+3"), Action::TimeScale3);
  in.bind_button(chord("Ctrl+4"), Action::TimeScale5);
  in.bind_button(chord("Ctrl+5"), Action::TimeScale10);

  // This is automatically bound to CancelInput in text input mode:
  in.bind_button(chord("Escape"), Action::Exit);

  in.bind_fixed(chord("Enter"), Action::SubmitInput);
  in.bind_fixed(chord("Backspace"), Action::Backspace);

  in.begin_section("Text input");
  in.bind_button(chord("Ctrl+U"), Action::EraseLine);

  in.begin_section("Menu and list navigation");
  in.bind_button(chord("PageUp"), Action::PageUp);
  in.bind_button(chord("PageDown"), Action::PageDown);

  in.begin_section("Moving and selecting");
  in.bind_button(chord("Space"), Action::Fire);
  in.bind_button(chord("Up"), Action::MoveUp);
  in.bind_button(chord("Down"), Action::MoveDown);
  in.bind_button(chord("Left"), Action::MoveLeft);
  in.bind_button(chord("Right"), Action::MoveRight);
  in.bind_gamepad_button(GamepadButton::DpadUp, Action::MoveUp);
  in.bind_gamepad_button(GamepadButton::DpadDown, Action::MoveDown);
  in.bind_gamepad_button(GamepadButton::DpadLeft, Action::MoveLeft);
  in.bind_gamepad_button(GamepadButton::DpadRight, Action::MoveRight);

  in.begin_section("Second player (during burglary)");
  in.bind_button(chord("W"), Action::P2_MoveUp);
  in.bind_button(chord("S"), Action::P2_MoveDown);
  in.bind_button(chord("A"), Action::P2_MoveLeft);
  in.bind_button(chord("D"), Action::P2_MoveRight);
  in.bind_button(chord("LCtrl"), Action::P2_Fire);
  in.bind_gamepad_button(GamepadButton::LeftStickUp, Action::P2_MoveUp);
  in.bind_gamepad_button(GamepadButton::LeftStickDown, Action::P2_MoveDown);
  in.bind_gamepad_button(GamepadButton::LeftStickLeft, Action::P2_MoveLeft);
  in.bind_gamepad_button(GamepadButton::LeftStickRight, Action::P2_MoveRight);
  in.bind_gamepad_button(GamepadButton::A, Action::P2_Fire);

  in.begin_section("Map scrolling");
  in.bind_button(chord("Up"), Action::ScrollUp);
  in.bind_button(chord("Down"), Action::ScrollDown);
  in.bind_button(chord("Left"), Action::ScrollLeft);
  in.bind_button(chord("Right"), Action::ScrollRight);

  in.begin_section("Recipes");
  in.bind_button(chord("F1"), Action::LoadRecipe);
  in.bind_button(chord("F2"), Action::SaveRecipe);

  in.begin_section("Switching restaurants");
  in.bind_button(chord("Tab"), Action::NextRestaurant);
  in.bind_button(chord("Shift+Tab"), Action::PreviousRestaurant);

  in.begin_section("Restaurant view");
  in.bind_button(chord("Space"), Action::ToggleRestaurantView);

  in.begin_section("Confirm popup");
  in.bind_button(chord("Y"), Action::ConfirmYes);
  in.bind_button(chord("N"), Action::ConfirmNo);

  in.begin_section("Menu items");
  in.bind_button(chord("Tab"), Action::NextMenuItem);
  in.bind_button(chord("Shift+Tab"), Action::PrevMenuItem);

  in.begin_section("Debug tooling");
  in.declare_unbound(Action::ToggleConsole);
  in.declare_unbound(Action::ToggleDebug);
  in.declare_unbound(Action::LoadCanonicalRecipe);
  in.declare_unbound(Action::CycleBackward);
  in.declare_unbound(Action::CycleForward);
  in.declare_unbound(Action::CycleUp);
  in.declare_unbound(Action::CycleDown);
  in.declare_unbound(Action::ToggleExtraInfo);
  in.declare_unbound(Action::MoveItemUp);
  in.declare_unbound(Action::MoveItemLeft);
  in.declare_unbound(Action::MoveItemDown);
  in.declare_unbound(Action::MoveItemRight);
  in.declare_unbound(Action::ChangeItem);
  in.declare_unbound(Action::ChangeItem2);
  in.declare_unbound(Action::HideItem);
}

void restore_default_bindings(InputManager& in,
                              const std::vector<Action>& actions) {
  InputManager defaults;
  configure_default_bindings(defaults);
  for (Action a : actions) {
    in.remove_bindings(a);
    for (const Binding& chord : defaults.button_bindings_for(a)) {
      in.bind_button(chord, a);
    }
    for (const auto& [button, action] : defaults.gamepad_button_bindings()) {
      if (action == a) {
        in.bind_gamepad_button(button, a);
      }
    }
  }
}

} // namespace input
