/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "engine/SDL_include.h"

// These are declared in this order so reverse order destruction goes the
// right way.
struct SDLContext {
  SDL_Window_unique_ptr window;
  SDL_Renderer_unique_ptr renderer;
  SDL_Texture_unique_ptr texture;
};
