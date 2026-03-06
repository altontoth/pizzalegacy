/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "engine/SDL_include.h"
#include "engine/input/Key.h"

namespace input::backend {

Key key_from_scancode(SDL_Scancode sc);
SDL_Scancode scancode_from_key(Key k);

} // namespace input::backend
