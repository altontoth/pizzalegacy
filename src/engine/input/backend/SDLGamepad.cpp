/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/input/backend/SDLGamepad.h"

namespace input::backend {

GamepadButton button_from_sdl(SDL_GameControllerButton b) {
  switch (b) {
  case SDL_CONTROLLER_BUTTON_A:
    return GamepadButton::A;
  case SDL_CONTROLLER_BUTTON_B:
    return GamepadButton::B;
  case SDL_CONTROLLER_BUTTON_X:
    return GamepadButton::X;
  case SDL_CONTROLLER_BUTTON_Y:
    return GamepadButton::Y;
  case SDL_CONTROLLER_BUTTON_BACK:
    return GamepadButton::Back;
  case SDL_CONTROLLER_BUTTON_GUIDE:
    return GamepadButton::Guide;
  case SDL_CONTROLLER_BUTTON_START:
    return GamepadButton::Start;
  case SDL_CONTROLLER_BUTTON_LEFTSTICK:
    return GamepadButton::LeftStick;
  case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
    return GamepadButton::RightStick;
  case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
    return GamepadButton::LeftShoulder;
  case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
    return GamepadButton::RightShoulder;
  case SDL_CONTROLLER_BUTTON_DPAD_UP:
    return GamepadButton::DpadUp;
  case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
    return GamepadButton::DpadDown;
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
    return GamepadButton::DpadLeft;
  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
    return GamepadButton::DpadRight;
  default:
    return GamepadButton::Unknown;
  }
}

SDL_GameControllerAxis axis_to_sdl(GamepadAxis a) {
  switch (a) {
  case GamepadAxis::LeftX:
    return SDL_CONTROLLER_AXIS_LEFTX;
  case GamepadAxis::LeftY:
    return SDL_CONTROLLER_AXIS_LEFTY;
  case GamepadAxis::RightX:
    return SDL_CONTROLLER_AXIS_RIGHTX;
  case GamepadAxis::RightY:
    return SDL_CONTROLLER_AXIS_RIGHTY;
  case GamepadAxis::TriggerLeft:
    return SDL_CONTROLLER_AXIS_TRIGGERLEFT;
  case GamepadAxis::TriggerRight:
    return SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
  case GamepadAxis::Unknown:
  default:
    return SDL_CONTROLLER_AXIS_INVALID;
  }
}

} // namespace input::backend
