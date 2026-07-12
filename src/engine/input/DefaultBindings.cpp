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

  in.bind_button(chord("Left"), Action::MoveLeft);
  in.bind_button(chord("Right"), Action::MoveRight);
  in.bind_button(chord("Up"), Action::MoveUp);
  in.bind_button(chord("Down"), Action::MoveDown);
  in.bind_gamepad_button(GamepadButton::DpadLeft, Action::MoveLeft);
  in.bind_gamepad_button(GamepadButton::DpadRight, Action::MoveRight);
  in.bind_gamepad_button(GamepadButton::DpadUp, Action::MoveUp);
  in.bind_gamepad_button(GamepadButton::DpadDown, Action::MoveDown);

  in.bind_button(chord("A"), Action::P2_MoveLeft);
  in.bind_button(chord("D"), Action::P2_MoveRight);
  in.bind_button(chord("W"), Action::P2_MoveUp);
  in.bind_button(chord("S"), Action::P2_MoveDown);
  in.bind_button(chord("LCtrl"), Action::P2_Fire);
  in.bind_gamepad_button(GamepadButton::LeftStickLeft, Action::P2_MoveLeft);
  in.bind_gamepad_button(GamepadButton::LeftStickRight, Action::P2_MoveRight);
  in.bind_gamepad_button(GamepadButton::LeftStickUp, Action::P2_MoveUp);
  in.bind_gamepad_button(GamepadButton::LeftStickDown, Action::P2_MoveDown);
  in.bind_gamepad_button(GamepadButton::A, Action::P2_Fire);

  in.bind_button(chord("Left"), Action::ScrollLeft);
  in.bind_button(chord("Right"), Action::ScrollRight);
  in.bind_button(chord("Up"), Action::ScrollUp);
  in.bind_button(chord("Down"), Action::ScrollDown);

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

  // Menu UI
  in.bind_button(chord("Tab"), Action::NextMenuItem);
  in.bind_button(chord("Shift+Tab"), Action::PrevMenuItem);
}

} // namespace input
