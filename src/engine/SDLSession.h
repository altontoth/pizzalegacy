/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/SDL_include.h"

/*
 * RAII owner of the SDL library lifetime.
 *
 * Construction bootstraps SDL with no subsystems (enough for SDL_GetPrefPath);
 * subsystems are added later via init_subsystems() once we know what we need.
 */
class SDLSession {
public:
  SDLSession() : initialized_(SDL_Init(0)) {}
  ~SDLSession() { SDL_Quit(); }

  SDLSession(const SDLSession&) = delete;
  SDLSession& operator=(const SDLSession&) = delete;
  SDLSession(SDLSession&&) = delete;
  SDLSession& operator=(SDLSession&&) = delete;
  bool initialized() const noexcept { return initialized_; }
  bool init_subsystems(SDL_InitFlags flags) { return SDL_InitSubSystem(flags); }

private:
  bool initialized_;
};
