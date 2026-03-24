/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/PlatformPaths.h"

#include <SDL2/SDL.h>
#include <iostream>

namespace platform_paths {

namespace {
std::string base_dir_;
} // namespace

void init() {
  char* pref = SDL_GetPrefPath("nl.pizzalegacy", "Pizza Legacy");
  if (pref) {
    base_dir_ = pref;
    SDL_free(pref);
  }
  else {
    // Fallback to current directory if SDL_GetPrefPath fails
    std::cerr << "Warning: SDL_GetPrefPath failed, using current directory\n";
    base_dir_ = "./";
  }
}

const std::string& base_dir() { return base_dir_; }

std::string config_file_path() { return base_dir_ + "plegacy.conf"; }

std::string saves_dir() { return base_dir_ + "saves/"; }

std::string screenshots_dir() { return base_dir_ + "screenshots/"; }

std::string pizzas_dir() { return base_dir_ + "pizzas/"; }

} // namespace platform_paths
