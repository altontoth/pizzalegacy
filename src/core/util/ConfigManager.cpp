/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/util/ConfigManager.h"

#include <filesystem>
#include <fstream>
#include <tinyfiledialogs/tinyfiledialogs.h>
#include <vector>
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

// Returns the directory containing PT.EXE if found anywhere under `base`,
// checking known Steam and DOSBox wrapper subdirectory layouts. Empty if not
// found.
static std::string probe_for_dos_game(const std::string& base) {
  namespace fs = std::filesystem;
  fs::path b(base);

  std::vector<fs::path> candidates;
  auto add = [&](auto&& p) { candidates.push_back(b / p); };

  add(".");
  add("game");
  add("Pizza Connection");
  add("Pizza Connection/game");

#ifdef __APPLE__
  add("Pizza Connection.app/Contents/Resources/game");
  add("Contents/Resources/game");
  add("Pizza Connection/Pizza Connection.app/Contents/Resources/game");
  add("steamapps/common/Pizza Connection/Pizza "
      "Connection.app/Contents/Resources/game");
#endif

  for (const auto& candidate : candidates) {
    if (fileutils::fexists((candidate / "PT.EXE").string())) {
      return candidate.string();
    }
  }
  return {};
}

#ifdef __APPLE__
static constexpr const char* k_location_hint =
    "Select the folder containing your original Pizza Tycoon / Pizza "
    "Connection game files.\n\n"
    "Steam users: you can point to your Steam directory, your steamapps/common "
    "folder, or the Pizza Connection folder. The game inside the app bundle "
    "will be found automatically.";
#elif defined(_WIN32)
static constexpr const char* k_location_hint =
    "Select the folder containing your original Pizza Tycoon / Pizza "
    "Connection game files.\n\n"
    "Steam users: you can point to your Steam directory, your "
    "steamapps\\common "
    "folder, or directly to the Pizza Connection folder.";
#else
static constexpr const char* k_location_hint =
    "Select the folder containing your original Pizza Tycoon / Pizza "
    "Connection game files.\n\n"
    "Steam users: you can point to your Steam directory, your steamapps/common "
    "folder, or the Pizza Connection folder.";
#endif

std::string ConfigManager::prompt_for_game_location() {
  tinyfd_messageBox("Note", k_location_hint, "ok", "info", 1);

  bool give_up = false;
  bool game_found = false;
  std::string selected_folder;
  std::string active_asset_format = "english";

  while (!give_up && !game_found) {
    const char* folder_name =
        tinyfd_selectFolderDialog("Select original game location", nullptr);
    if (!folder_name) {
      give_up = true;
    }
    else {
      std::string dos_path = probe_for_dos_game(folder_name);
      if (!dos_path.empty()) {
        selected_folder = dos_path;
        game_found = true;
        Logger::info("Detected DOS version (PT.EXE) at: {}", dos_path);
      }
      else {
        tinyfd_messageBox(
            "Error",
            "Could not find PT.EXE in this directory or any known "
            "Steam/DOSBox subdirectory.",
            "ok", "error", 1);
      }
    }
  }

  if (!game_found) {
    return std::string{};
  }

  value("active_asset_format", active_asset_format);
  value("game_path_english", selected_folder);

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
  save_bool("rmb_drag_inverted", opts.rmb_drag_inverted);
  save_bool("scroll_zoom_inverted", opts.scroll_zoom_inverted);

  config_["active_asset_format"] =
      (opts.asset_format == platform::AssetFormat::GERMAN) ? "german"
                                                           : "english";

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
  load_bool("rmb_drag_inverted", opts.rmb_drag_inverted);
  load_bool("scroll_zoom_inverted", opts.scroll_zoom_inverted);

  auto it = config_.find("active_asset_format");
  if (it != config_.end()) {
    opts.asset_format = (it->second == "german")
                            ? platform::AssetFormat::GERMAN
                            : platform::AssetFormat::ENGLISH;
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
      (format == "german") ? "game_path_german" : "game_path_english";

  auto it = config_.find(key);
  if (it != config_.end()) {
    return it->second;
  }

  return "";
}

std::string ConfigManager::active_game_path() const {
  std::string format = value("active_asset_format");
  if (format.empty()) {
    format = "english";
  }

  return game_path_for_format(format);
}

ConfigManager::FormatAvailability
ConfigManager::detect_available_formats() const {
  FormatAvailability result{.english_available = false,
                            .german_available = false,
                            .english_path = "",
                            .german_path = ""};

  // Check game_path_english if set
  auto it = config_.find("game_path_english");
  if (it != config_.end() && !result.english_available) {
    std::string path = it->second;
    if (fileutils::fexists(path + "/PT.EXE")) {
      result.english_available = true;
      result.english_path = path;
    }
  }

  return result;
}

bool ConfigManager::both_formats_available() const {
  auto availability = detect_available_formats();
  return availability.english_available && availability.german_available;
}
