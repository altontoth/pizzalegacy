/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/input/backend/SDLGamepad.h"

namespace input::backend {

GamepadButton button_from_sdl(SDL_GamepadButton b) {
  switch (b) {
  case SDL_GAMEPAD_BUTTON_SOUTH:
    return GamepadButton::A;
  case SDL_GAMEPAD_BUTTON_EAST:
    return GamepadButton::B;
  case SDL_GAMEPAD_BUTTON_WEST:
    return GamepadButton::X;
  case SDL_GAMEPAD_BUTTON_NORTH:
    return GamepadButton::Y;
  case SDL_GAMEPAD_BUTTON_BACK:
    return GamepadButton::Back;
  case SDL_GAMEPAD_BUTTON_GUIDE:
    return GamepadButton::Guide;
  case SDL_GAMEPAD_BUTTON_START:
    return GamepadButton::Start;
  case SDL_GAMEPAD_BUTTON_LEFT_STICK:
    return GamepadButton::LeftStick;
  case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
    return GamepadButton::RightStick;
  case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
    return GamepadButton::LeftShoulder;
  case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
    return GamepadButton::RightShoulder;
  case SDL_GAMEPAD_BUTTON_DPAD_UP:
    return GamepadButton::DpadUp;
  case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
    return GamepadButton::DpadDown;
  case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
    return GamepadButton::DpadLeft;
  case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
    return GamepadButton::DpadRight;
  default:
    return GamepadButton::Unknown;
  }
}

SDL_GamepadAxis axis_to_sdl(GamepadAxis a) {
  switch (a) {
  case GamepadAxis::LeftX:
    return SDL_GAMEPAD_AXIS_LEFTX;
  case GamepadAxis::LeftY:
    return SDL_GAMEPAD_AXIS_LEFTY;
  case GamepadAxis::RightX:
    return SDL_GAMEPAD_AXIS_RIGHTX;
  case GamepadAxis::RightY:
    return SDL_GAMEPAD_AXIS_RIGHTY;
  case GamepadAxis::TriggerLeft:
    return SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
  case GamepadAxis::TriggerRight:
    return SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
  case GamepadAxis::Unknown:
  default:
    return SDL_GAMEPAD_AXIS_INVALID;
  }
}

} // namespace input::backend
