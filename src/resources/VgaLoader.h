/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>

#include "gfx/Surface.h"

class ConfigManager;
using gfx::Surface;

class VgaLoader {
public:
  static Surface load(const std::string& filename);

  // Set the ConfigManager reference for resource platform decision
  // Should be called during Game::init() before any VGA files are loaded
  static void config_manager(ConfigManager* config);

private:
  static ConfigManager* s_config_manager;
};
