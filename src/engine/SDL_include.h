/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <SDL2/SDL.h>
#include <memory>

struct SDL_SurfaceDeleter {
  void operator()(SDL_Surface* s) const {
    if (s) {
      SDL_FreeSurface(s);
    }
  }
};

struct SDL_CursorDeleter {
  void operator()(SDL_Cursor* c) const {
    if (c) {
      SDL_FreeCursor(c);
    }
  }
};

struct SDL_PixelFormatDeleter {
  void operator()(SDL_PixelFormat* f) const {
    if (f) {
      SDL_FreeFormat(f);
    }
  }
};

using SDL_Surface_unique_ptr = std::unique_ptr<SDL_Surface, SDL_SurfaceDeleter>;
using SDL_Cursor_unique_ptr = std::unique_ptr<SDL_Cursor, SDL_CursorDeleter>;
using SDL_PixelFormat_unique_ptr =
    std::unique_ptr<SDL_PixelFormat, SDL_PixelFormatDeleter>;
