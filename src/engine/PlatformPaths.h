/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>

namespace platform_paths {

// Must be called after SDL_Init(). Resolves the platform-appropriate
// preferences directory via SDL_GetPrefPath.
void init();

const std::string& base_dir();
std::string config_file_path();
std::string saves_dir();
std::string screenshots_dir();
std::string recipes_dir();
std::string missions_dir();

} // namespace platform_paths
