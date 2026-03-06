/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "engine/SDL_include.h"

struct SDLContext {
  SDL_Window* window{nullptr};
  SDL_Renderer* renderer{nullptr};
  SDL_Texture* texture{nullptr};
};
