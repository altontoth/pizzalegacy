/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <map>
#include <string>
#include "core/state/GameOptions.h"

class ConfigManager {
public:
  explicit ConfigManager(std::string filename = "./settings.conf");

  bool load_config();

  void save_config();
  std::string prompt_for_game_location();
  const std::string value(const std::string& key) const;
  void value(const std::string& key, const std::string& value);
  void save_options(const GameOptions& opts);
  GameOptions load_options() const;

  struct FormatAvailability {
    bool english_available;
    bool german_available;
    std::string english_path;
    std::string german_path;
  };

  std::string game_path_for_format(const std::string& format) const;
  std::string active_game_path() const;
  FormatAvailability detect_available_formats() const;
  bool both_formats_available() const;
  // Migrate old single-path config to new dual-path system
  void migrate_legacy_game_path();

private:
  std::string filename_;
  std::map<std::string, std::string> config_;
};
