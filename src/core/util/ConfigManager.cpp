/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/util/ConfigManager.h"

#include <fstream>
#include <tinyfiledialogs/tinyfiledialogs.h>
#include "core/state/GameState.h"
#include "engine/Fatal.h"
#include "resources/util/FileUtils.h"
#include "resources/util/Platform.h"

ConfigManager* ConfigManager::s_instance_ = nullptr;

void ConfigManager::set_instance(ConfigManager* instance) {
  s_instance_ = instance;
}

ConfigManager* ConfigManager::instance() { return s_instance_; }

ConfigManager::ConfigManager(std::string filename)
    : filename_(std::move(filename)) {}

bool ConfigManager::load_config() {
  if (!fileutils::fexists(filename_)) {
    return false;
  }

  std::ifstream file(filename_);
  if (!file.is_open()) {
    return false;
  }

  std::string line;
  while (std::getline(file, line)) {
    auto delimiterPos = line.find('=');
    if (delimiterPos == std::string::npos) {
      continue;
    }
    std::string key = line.substr(0, delimiterPos);
    std::string value = line.substr(delimiterPos + 1);
    config_[key] = value;
  }
  options_ = load_options();

  migrate_legacy_game_path();

  return true;
}

void ConfigManager::save_config() {
  std::ofstream file(filename_);
  if (!file.is_open()) {
    fatal("Unable to open config file for writing");
  }

  for (const auto& [key, value] : config_) {
    file << key << "=" << value << "\n";
  }
}

std::string ConfigManager::prompt_for_game_location() {
  tinyfd_messageBox("Note",
                    "Please select the directory where to find the "
                    "original Pizza Tycoon or Pizza Connection game.",
                    "ok", "info", 1);

  bool give_up = false;
  bool game_found = false;
  std::string selected_folder;
  std::string active_asset_format = "dos";

  while (!give_up && !game_found) {
    const char* folder_name =
        tinyfd_selectFolderDialog("Select original game location", nullptr);
    if (!folder_name) {
      give_up = true;
    }
    else {
      selected_folder = folder_name;

      // Check for Amiga version first (PC1.2)
      std::string amiga_exe = selected_folder + "/PC1.2";
      if (fileutils::fexists(amiga_exe)) {
        game_found = true;
        active_asset_format = "amiga";
        Logger::info("Detected Amiga version (PC1.2)");
      }
      else {
        // Check for DOS version (PT.EXE)
        std::string dos_exe = selected_folder + "/PT.EXE";
        if (fileutils::fexists(dos_exe)) {
          game_found = true;
          active_asset_format = "dos";
          Logger::info("Detected DOS version (PT.EXE)");
        }
        else {
          tinyfd_messageBox(
              "Error",
              "Could not find PT.EXE (DOS) or PC1.2 (Amiga) in this directory.",
              "ok", "error", 1);
        }
      }
    }
  }

  if (!game_found) {
    return std::string{};
  }

  value("active_asset_format", active_asset_format);

  std::string pt_exe = selected_folder + "/PT.EXE";
  std::string pc1_2 = selected_folder + "/PC1.2";

  if (fileutils::fexists(pt_exe)) {
    value("game_path_dos", selected_folder);
  }
  if (fileutils::fexists(pc1_2)) {
    value("game_path_amiga", selected_folder);
  }

  return selected_folder;
}

const std::string ConfigManager::value(const std::string& key) const {
  static const std::string empty{};
  auto it = config_.find(key);
  if (it != config_.end()) {
    return it->second;
  }
  return empty;
}

void ConfigManager::value(const std::string& key, const std::string& value) {
  config_[key] = value;
}

static std::string bool_to_string(bool val) { return val ? "true" : "false"; }

static bool string_to_bool(const std::string& str) {
  return str == "true" || str == "1";
}

void ConfigManager::save_options(const GameOptions& opts) {
  options_ = opts;

  auto save_bool = [this](const std::string& key, bool value) {
    config_[key] = bool_to_string(value);
  };

  save_bool("map_fx", opts.map_fx);
  save_bool("restaurant_fx", opts.restaurant_fx);
  save_bool("music", opts.music);
  save_bool("automatic_report", opts.automatic_report);
  save_bool("crt_aspect_ratio", opts.crt_aspect_ratio);
  save_bool("fix_legacy_bugs", opts.fix_legacy_bugs);
  save_bool("qol_improvements", opts.qol_improvements);

  const char* format_str =
      (opts.asset_format == platform::AssetFormat::AMIGA) ? "amiga" : "dos";
  config_["active_asset_format"] = format_str;

  config_["resolution"] = std::string(to_string(opts.resolution));

  save_config();
}

GameOptions ConfigManager::load_options() const {
  GameOptions opts;

  auto load_bool = [this](const std::string& key, bool& target) {
    auto it = config_.find(key);
    if (it != config_.end()) {
      target = string_to_bool(it->second);
    }
  };

  load_bool("map_fx", opts.map_fx);
  load_bool("restaurant_fx", opts.restaurant_fx);
  load_bool("music", opts.music);
  load_bool("automatic_report", opts.automatic_report);
  load_bool("crt_aspect_ratio", opts.crt_aspect_ratio);
  load_bool("fix_legacy_bugs", opts.fix_legacy_bugs);
  load_bool("qol_improvements", opts.qol_improvements);

  auto it = config_.find("active_asset_format");
  if (it != config_.end()) {
    opts.asset_format = (it->second == "amiga") ? platform::AssetFormat::AMIGA
                                                : platform::AssetFormat::DOS;
  }

  auto res_it = config_.find("resolution");
  if (res_it != config_.end()) {
    if (res_it->second == "1280x800") {
      opts.resolution = ScreenResolution::Res_1280x800;
    }
    else {
      opts.resolution = ScreenResolution::Res_640x400;
    }
  }

  return opts;
}

std::string
ConfigManager::game_path_for_format(const std::string& format) const {
  const std::string key =
      (format == "dos") ? "game_path_dos" : "game_path_amiga";

  auto it = config_.find(key);
  if (it != config_.end()) {
    return it->second;
  }

  it = config_.find("game_path");
  if (it != config_.end()) {
    return it->second;
  }

  return "";
}

std::string ConfigManager::active_game_path() const {
  std::string format = value("active_asset_format");
  if (format.empty()) {
    format = "dos";
  }

  return game_path_for_format(format);
}

ConfigManager::FormatAvailability
ConfigManager::detect_available_formats() const {
  FormatAvailability result{false, false, "", ""};

  // Check game_path_dos if set
  auto it = config_.find("game_path_dos");
  if (it != config_.end()) {
    std::string dos_path = it->second;
    std::string pt_exe = dos_path + "/PT.EXE";
    if (fileutils::fexists(pt_exe)) {
      result.dos_available = true;
      result.dos_path = dos_path;
    }
  }

  it = config_.find("game_path_amiga");
  if (it != config_.end()) {
    std::string amiga_path = it->second;
    std::string pc1_2 = amiga_path + "/PC1.2";
    if (fileutils::fexists(pc1_2)) {
      result.amiga_available = true;
      result.amiga_path = amiga_path;
    }
  }

  it = config_.find("game_path");
  if (it != config_.end()) {
    std::string legacy_path = it->second;

    if (!result.dos_available) {
      std::string pt_exe = legacy_path + "/PT.EXE";
      if (fileutils::fexists(pt_exe)) {
        result.dos_available = true;
        result.dos_path = legacy_path;
      }
    }

    if (!result.amiga_available) {
      std::string pc1_2 = legacy_path + "/PC1.2";
      if (fileutils::fexists(pc1_2)) {
        result.amiga_available = true;
        result.amiga_path = legacy_path;
      }
    }
  }

  return result;
}

bool ConfigManager::both_formats_available() const {
  auto availability = detect_available_formats();
  return availability.dos_available && availability.amiga_available;
}

void ConfigManager::migrate_legacy_game_path() {
  auto it = config_.find("game_path");
  if (it != config_.end() && config_.find("game_path_dos") == config_.end() &&
      config_.find("game_path_amiga") == config_.end()) {

    std::string legacy_path = it->second;

    // Detect what formats exist at legacy path
    std::string pt_exe = legacy_path + "/PT.EXE";
    std::string pc1_2 = legacy_path + "/PC1.2";

    bool has_dos = fileutils::fexists(pt_exe);
    bool has_amiga = fileutils::fexists(pc1_2);

    // Set new keys based on what we found
    if (has_dos) {
      config_["game_path_dos"] = legacy_path;
    }
    if (has_amiga) {
      config_["game_path_amiga"] = legacy_path;
    }

    if (has_dos || has_amiga) {
      config_.erase("game_path");
      save_config();
    }
  }
}
