/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/PlatformPaths.h"

#include <SDL3/SDL.h>
#include <iostream>

namespace platform_paths {

namespace {
std::string base_dir_;
std::string data_dir_;
} // anonymous namespace

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

  const char* base = SDL_GetBasePath();
  if (base) {
    data_dir_ = std::string(base) + "data/";
  }
  else {
    std::cerr << "Warning: SDL_GetBasePath failed, using current directory\n";
    data_dir_ = "./data/";
  }
}

const std::string& base_dir() { return base_dir_; }

const std::string& data_dir() { return data_dir_; }

std::string config_file_path() { return base_dir_ + "settings.conf"; }

std::string bindings_file_path() { return base_dir_ + "bindings.conf"; }

std::string saves_dir() { return base_dir_ + "saves/"; }

std::string screenshots_dir() { return base_dir_ + "screenshots/"; }

std::string recipes_dir() { return base_dir_ + "recipes/"; }

std::string missions_dir() { return base_dir_ + "missions/"; }

} // namespace platform_paths
