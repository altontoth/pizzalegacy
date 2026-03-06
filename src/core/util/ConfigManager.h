/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <map>
#include <string>
#include "core/state/GameState.h"

class ConfigManager {
public:
  explicit ConfigManager(std::string filename = "./plegacy.conf");

  bool load_config();

  static void set_instance(ConfigManager* instance);
  static ConfigManager* instance();
  void save_config();
  std::string prompt_for_game_location();
  const std::string value(const std::string& key) const;
  void value(const std::string& key, const std::string& value);
  const GameOptions& options() const { return options_; }
  GameOptions& options() { return options_; }

  void save_options(const GameOptions& opts);
  GameOptions load_options() const;

  struct FormatAvailability {
    bool dos_available;
    bool amiga_available;
    std::string dos_path;
    std::string amiga_path;
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
  GameOptions options_;
  static ConfigManager* s_instance_;
};
