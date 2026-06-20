/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cassert>
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

  // Global/UI
  in.bind_button(chord("Backtick"), Action::ToggleConsole);
  in.bind_button(chord("Ctrl+D"), Action::ToggleDebug);
  in.bind_button(chord("Ctrl+S"), Action::TakeScreenshot);
  in.bind_button(chord("Ctrl+F"), Action::ToggleFullscreen);

  // This is automatically bound to CancelInput in text input mode:
  in.bind_button(chord("Escape"), Action::Exit);
  // Text input controls (work inside text mode)
  in.bind_button(chord("Enter"), Action::SubmitInput);
  in.bind_button(chord("Backspace"), Action::Backspace);
  in.bind_button(chord("Ctrl+U"), Action::EraseLine);

  in.bind_button(chord("Ctrl+P"), Action::TogglePause);

  // Menu / list navigation
  in.bind_button(chord("PageUp"), Action::PageUp);
  in.bind_button(chord("PageDown"), Action::PageDown);

  in.bind_axis_keys(Action::MoveVec2, Key::Up, Key::Down, Key::Left,
                    Key::Right);

  in.bind_button(chord("Left"), Action::MoveLeft);
  in.bind_button(chord("Right"), Action::MoveRight);
  in.bind_button(chord("Up"), Action::MoveUp);
  in.bind_button(chord("Down"), Action::MoveDown);

  in.bind_axis_keys(Action::P2_MoveVec2, Key::W, Key::S, Key::A, Key::D);
  in.bind_button(chord("LCtrl"), Action::P2_Fire);
  in.bind_gamepad_stick(Action::P2_MoveVec2, GamepadAxis::LeftX,
                        GamepadAxis::LeftY);
  in.bind_gamepad_button(GamepadButton::A, Action::P2_Fire);

  in.bind_axis_keys(Action::ScrollVec2, Key::Up, Key::Down, Key::Left,
                    Key::Right);

  in.bind_button(chord("Space"), Action::Fire);

  // Debug tooling:
  in.bind_button(chord("Left"), Action::CycleBackward);
  in.bind_button(chord("Right"), Action::CycleForward);
  in.bind_button(chord("Up"), Action::CycleUp);
  in.bind_button(chord("Down"), Action::CycleDown);
  in.bind_button(chord("Ctrl+N"), Action::ToggleExtraInfo);
  in.bind_button(chord("W"), Action::MoveItemUp);
  in.bind_button(chord("A"), Action::MoveItemLeft);
  in.bind_button(chord("S"), Action::MoveItemDown);
  in.bind_button(chord("D"), Action::MoveItemRight);
  in.bind_button(chord("E"), Action::ChangeItem);
  in.bind_button(chord("T"), Action::ChangeItem2);
  in.bind_button(chord("Ctrl+L"), Action::LockItem);
  in.bind_button(chord("Ctrl+H"), Action::HideItem);

  in.bind_button(chord("F1"), Action::LoadRecipe);
  in.bind_button(chord("F2"), Action::SaveRecipe);
  in.bind_button(chord("F3"), Action::LoadCanonicalRecipe);

  in.bind_button(chord("Tab"), Action::NextRestaurant);
  in.bind_button(chord("Shift+Tab"), Action::PreviousRestaurant);
  in.bind_button(chord("Space"), Action::ToggleRestaurantView);

  // Confirm popup
  in.bind_button(chord("Y"), Action::ConfirmYes);
  in.bind_button(chord("Enter"), Action::ConfirmYes);
  in.bind_button(chord("N"), Action::ConfirmNo);
}

} // namespace input
