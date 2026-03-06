/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/SDL_include.h"
#include "engine/geom/Spaces.h"

namespace SDLAdapter {
inline SDL_Rect to_sdl_rect(const ViewRect& rect) {
  SDL_Rect sdl_rect;
  sdl_rect.x = rect.x;
  sdl_rect.y = rect.y;
  sdl_rect.w = rect.w;
  sdl_rect.h = rect.h;
  return sdl_rect;
}

inline ViewRect from_sdl_rect(const SDL_Rect& sdl_rect) {
  return ViewRect(sdl_rect.x, sdl_rect.y, sdl_rect.w, sdl_rect.h);
}
} // namespace SDLAdapter
