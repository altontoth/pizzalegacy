/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>

#include "engine/SDL_include.h"

namespace engine {

// A bare name goes in the screenshots directory; a path with a directory in
// it is used unchanged.
std::string resolve_screenshot_path(const std::string& name);

bool save_screenshot(SDL_Surface* surface, const std::string& path);

} // namespace engine
