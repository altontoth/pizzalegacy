/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/SDL_include.h"
#include "engine/input/Gamepad.h"

namespace input::backend {

GamepadButton button_from_sdl(SDL_GamepadButton b);
SDL_GamepadAxis axis_to_sdl(GamepadAxis a);

} // namespace input::backend
