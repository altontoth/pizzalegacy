/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <SDL3/SDL.h>
#include <memory>

struct SDL_SurfaceDeleter {
  void operator()(SDL_Surface* s) const {
    if (s) {
      SDL_DestroySurface(s);
    }
  }
};

struct SDL_CursorDeleter {
  void operator()(SDL_Cursor* c) const {
    if (c) {
      SDL_DestroyCursor(c);
    }
  }
};

struct SDL_WindowDeleter {
  void operator()(SDL_Window* w) const {
    if (w) {
      SDL_DestroyWindow(w);
    }
  }
};

struct SDL_RendererDeleter {
  void operator()(SDL_Renderer* r) const {
    if (r) {
      SDL_DestroyRenderer(r);
    }
  }
};

struct SDL_TextureDeleter {
  void operator()(SDL_Texture* t) const {
    if (t) {
      SDL_DestroyTexture(t);
    }
  }
};

using SDL_Surface_unique_ptr = std::unique_ptr<SDL_Surface, SDL_SurfaceDeleter>;
using SDL_Cursor_unique_ptr = std::unique_ptr<SDL_Cursor, SDL_CursorDeleter>;
using SDL_Window_unique_ptr = std::unique_ptr<SDL_Window, SDL_WindowDeleter>;
using SDL_Renderer_unique_ptr =
    std::unique_ptr<SDL_Renderer, SDL_RendererDeleter>;
using SDL_Texture_unique_ptr = std::unique_ptr<SDL_Texture, SDL_TextureDeleter>;
