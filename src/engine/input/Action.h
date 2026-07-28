/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

namespace input {

enum class Action {
  // Global/UI
  ToggleConsole,
  ToggleDebug,
  TogglePause,
  ToggleFullscreen,
  TakeScreenshot,
  Exit,

  // Debug fast-forward
  TimeScale1,
  TimeScale2,
  TimeScale3,
  TimeScale5,
  TimeScale10,

  // List navigation
  PageUp,
  PageDown,

  // Player actions
  MoveLeft,
  MoveRight,
  MoveUp,
  MoveDown,
  MoveVec2,
  Fire,

  // Player actions
  P2_MoveLeft,
  P2_MoveRight,
  P2_MoveUp,
  P2_MoveDown,
  P2_MoveVec2,
  P2_Fire,

  // Map movement
  ScrollLeft,
  ScrollRight,
  ScrollUp,
  ScrollDown,
  ScrollVec2,

  // Restaurant Toggles
  NextRestaurant,
  PreviousRestaurant,
  ToggleRestaurantView,

  // Input
  SubmitInput,
  Backspace,
  EraseLine, // control-U
  CancelInput,

  // Viewers
  CycleForward,
  CycleBackward,
  CycleUp,
  CycleDown,
  ToggleExtraInfo,
  MoveItemUp,
  MoveItemLeft,
  MoveItemRight,
  MoveItemDown,
  ChangeItem,
  ChangeItem2,
  HideItem,

  LoadRecipe,
  SaveRecipe,
  LoadCanonicalRecipe,

  // Confirm popup
  ConfirmYes,
  ConfirmNo,

  // Menu UI
  NextMenuItem,
  PrevMenuItem,
};

} // namespace input
