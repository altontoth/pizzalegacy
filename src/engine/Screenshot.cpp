/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/Screenshot.h"

#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

#include "engine/Logger.h"
#include "engine/PlatformPaths.h"
#include "engine/SDL_include.h"

namespace engine {
namespace {

SDL_Surface* format_for_png_fixed(SDL_Surface* input) {
  // Create a surface with explicitly defined RGBA masks;
  // when using SDL_PNGFormatAlpha something was wrong with the colors
  SDL_Surface* output =
      SDL_CreateSurface(input->w, input->h, SDL_PIXELFORMAT_ABGR8888);

  if (!output) {
    Logger::warn("Failed to create RGB surface: {}", SDL_GetError());
    return nullptr;
  }

  if (!SDL_BlitSurface(input, nullptr, output, nullptr)) {
    Logger::warn("Failed to blit surface: {}", SDL_GetError());
    SDL_DestroySurface(output);
    return nullptr;
  }

  return output;
}

std::string generate_screenshot_filename() {
  std::time_t t = std::time(nullptr);
  std::tm tm;
#if defined(_WIN32)
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif

  std::ostringstream oss;
  oss << "screenshot_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".png";
  return oss.str();
}

} // namespace

std::string resolve_screenshot_path(const std::string& name) {
  namespace fs = std::filesystem;
  if (name.empty()) {
    return platform_paths::screenshots_dir() + generate_screenshot_filename();
  }
  if (fs::path(name).has_parent_path()) {
    return name;
  }
  return platform_paths::screenshots_dir() + name;
}

bool save_screenshot(SDL_Surface* surface, const std::string& path) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::path(path).parent_path();
  if (!dir.empty() && !fs::exists(dir)) {
    fs::create_directories(dir);
  }
  SDL_Surface* tmp = format_for_png_fixed(surface);
  if (!tmp) {
    Logger::warn("Failed to format surface for PNG");
    return false;
  }
  SDL_Surface_unique_ptr png_surface(tmp);
  if (!SDL_SavePNG(png_surface.get(), path.c_str())) {
    Logger::warn("Failed to save PNG: {}", SDL_GetError());
    return false;
  }
  Logger::info("Screenshot saved as {}", path);
  return true;
}

} // namespace engine
