/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/util/ConfigManager.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <tinyfiledialogs/tinyfiledialogs.h>
#include <vector>
#include "core/state/GameOptions.h"
#include "engine/Fatal.h"
#include "engine/Logger.h"
#include "engine/StringUtil.h"
#include "resources/util/FileUtils.h"

ConfigManager::ConfigManager(std::string filename)
    : filename_(std::move(filename)) {}

namespace { // anonymous

std::string from_bool(bool value) { return value ? "true" : "false"; }

bool to_bool(const std::string& text) { return text == "true" || text == "1"; }

struct OptionDef {
  std::string_view key;
  std::string (*get)(const GameOptions&);
  void (*set)(GameOptions&, const std::string&);
  std::string_view section;
  std::string_view note;
};

template <auto Field>
OptionDef bool_option(std::string_view key, std::string_view section = {},
                      std::string_view note = {}) {
  return {key, [](const GameOptions& o) { return from_bool(o.*Field); },
          [](GameOptions& o, const std::string& s) { o.*Field = to_bool(s); },
          section, note};
}

template <auto Field>
OptionDef float_option(std::string_view key, std::string_view section = {},
                       std::string_view note = {}) {
  return {key, [](const GameOptions& o) { return std::to_string(o.*Field); },
          [](GameOptions& o, const std::string& s) {
            o.*Field = std::strtof(s.c_str(), nullptr);
          },
          section, note};
}

template <auto Field, int Lowest, int Highest>
OptionDef int_option(std::string_view key, std::string_view section = {},
                     std::string_view note = {}) {
  return {key, [](const GameOptions& o) { return std::to_string(o.*Field); },
          [](GameOptions& o, const std::string& s) {
            int parsed = 0;
            auto [ptr, ec] =
                std::from_chars(s.data(), s.data() + s.size(), parsed);
            if (ec == std::errc{}) {
              o.*Field = std::clamp(parsed, Lowest, Highest);
            }
          },
          section, note};
}

// Parser is the matching *_from_config_value function. Returns nullopt for a
// value it doesn't recognise.
template <auto Field, auto Parser>
OptionDef enum_option(std::string_view key, std::string_view section = {},
                      std::string_view note = {}) {
  return {key,
          [](const GameOptions& o) {
            return std::string(to_config_value(o.*Field));
          },
          [](GameOptions& o, const std::string& s) {
            if (auto parsed = Parser(s)) {
              o.*Field = *parsed;
            }
          },
          section, note};
}

const auto option_defs = std::to_array({
    bool_option<&GameOptions::crt_aspect_ratio>(
        "crt_aspect_ratio", "Display", "Use the original 4:3 CRT aspect ratio"),
    bool_option<&GameOptions::double_size>("double_size", {},
                                           "Render at 2x scale"),
    bool_option<&GameOptions::fullscreen>("fullscreen"),
    enum_option<&GameOptions::render_fps, &fps_option_from_config_value>(
        "render_fps", {}, "30, 60 or 120"),
    bool_option<&GameOptions::vsync>("vsync"),
    bool_option<&GameOptions::software_cursor>("software_cursor"),

    bool_option<&GameOptions::map_fx>("map_fx", "Sound"),
    bool_option<&GameOptions::restaurant_fx>("restaurant_fx"),
    bool_option<&GameOptions::music>("music"),
    float_option<&GameOptions::sound_volume>("sound_volume", {}, "0.0 to 1.0"),
    float_option<&GameOptions::music_volume>("music_volume", {}, "0.0 to 1.0"),

    bool_option<&GameOptions::rmb_drag_inverted>("rmb_drag_inverted", "Input"),
    bool_option<&GameOptions::scroll_zoom_inverted>("scroll_zoom_inverted"),
    int_option<&GameOptions::key_repeat_delay_ms,
               GameOptions::KEY_REPEAT_DELAY_MIN,
               GameOptions::KEY_REPEAT_DELAY_MAX>(
        "key_repeat_delay_ms", {},
        "milliseconds before a key you're holding down starts repeating; no "
        "options screen entry"),
    int_option<&GameOptions::key_repeat_interval_ms,
               GameOptions::KEY_REPEAT_INTERVAL_MIN,
               GameOptions::KEY_REPEAT_INTERVAL_MAX>(
        "key_repeat_interval_ms", {},
        "milliseconds between repeats; no options screen entry"),

    enum_option<&GameOptions::autosave_frequency,
                &autosave_frequency_from_config_value>(
        "autosave_frequency", "Saving", "off, daily, weekly or monthly"),
    int_option<&GameOptions::autosave_count, GameOptions::AUTOSAVE_COUNT_MIN,
               GameOptions::AUTOSAVE_COUNT_MAX>("autosave_count", {},
                                                "1 to 9 rotating slots"),
    bool_option<&GameOptions::load_list_sort_by_date>("load_list_sort_by_date"),
    bool_option<&GameOptions::load_list_show_autosaves>(
        "load_list_show_autosaves"),

    bool_option<&GameOptions::automatic_report>("automatic_report", "Gameplay"),
    bool_option<&GameOptions::fix_legacy_bugs>("fix_legacy_bugs"),
    bool_option<&GameOptions::qol_improvements>("qol_improvements"),
    bool_option<&GameOptions::mafia_shakedowns_target_all_restaurants>(
        "mafia_shakedowns_target_all_restaurants", {},
        "shake down a random restaurant instead of always the first; no "
        "options screen entry"),
    enum_option<&GameOptions::win_condition, &win_condition_from_config_value>(
        "win_condition", {},
        "paris, any_city or all_cities; no options screen entry"),
});

// Settings that take a string instead of being one of a set of options.
constexpr std::string_view non_option_keys[] = {"game_path_english"};

bool known_key(std::string_view key) {
  const auto matches = [key](std::string_view known) { return known == key; };
  return std::ranges::any_of(non_option_keys, matches) ||
         std::ranges::any_of(option_defs, matches, &OptionDef::key);
}

constexpr std::string_view settings_preamble =
    R"(# Pizza Legacy settings
#
# Each uncommented line sets one option:
#
#   option=value
#
# Only options you change need a line; anything absent uses the defaults shown
# below. If this file is missing it will be regenerated with the defaults on
# relaunch.
#
# Defaults:
)";

} // namespace

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
    const std::string_view stripped = util::trim(line);
    if (stripped.empty() || stripped.starts_with('#')) {
      continue;
    }
    const auto delimiterPos = stripped.find('=');
    if (delimiterPos == std::string_view::npos) {
      continue;
    }
    std::string key(util::trim(stripped.substr(0, delimiterPos)));
    if (!known_key(key)) {
      Logger::warn("{}: dropping unknown setting '{}'", filename_, key);
      continue;
    }
    std::string value(util::trim(stripped.substr(delimiterPos + 1)));
    config_[std::move(key)] = std::move(value);
  }
  return true;
}

void ConfigManager::save_config() {
  std::ofstream file(filename_);
  if (!file.is_open()) {
    fatal("Unable to open config file for writing");
  }

  file << settings_preamble;

  const GameOptions defaults;
  for (const auto& def : option_defs) {
    if (!def.section.empty()) {
      file << "#\n# " << def.section << ":\n";
    }
    if (!def.note.empty()) {
      file << "# " << def.note << "\n";
    }
    file << '#' << def.key << '=' << def.get(defaults) << '\n';
  }
  file << "\n";

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
  add("steamapps/common/Pizza Connection");
  add("steamapps/common/Pizza Connection/game");

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
static constexpr const char* location_hint =
    "Select the folder containing your original Pizza Tycoon / Pizza "
    "Connection game files.\n\n"
    "Steam users: you can point to your Steam directory, your steamapps/common "
    "folder, or the Pizza Connection folder. The game inside the app bundle "
    "will be found automatically.";
#elif defined(_WIN32)
static constexpr const char* location_hint =
    "Select the folder containing your original Pizza Tycoon / Pizza "
    "Connection game files.\n\n"
    "Steam users: you can point to your Steam directory, your "
    "steamapps\\common folder, or directly to the Pizza Connection folder.\n\n"
    "GOG users: point to your GOG Galaxy directory or directly to the Pizza "
    "Connection folder.";
#else
static constexpr const char* location_hint =
    "Select the folder containing your original Pizza Tycoon / Pizza "
    "Connection game files.\n\n"
    "Steam users: you can point to your Steam directory, your steamapps/common "
    "folder, or the Pizza Connection folder.";
#endif

std::string ConfigManager::prompt_for_game_location() {
  tinyfd_messageBox("Note", location_hint, "ok", "info", 1);

  bool give_up = false;
  bool game_found = false;
  std::string selected_folder;

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

void ConfigManager::save_options(const GameOptions& opts) {
  const GameOptions defaults;
  for (const auto& def : option_defs) {
    std::string value = def.get(opts);
    if (value == def.get(defaults)) {
      config_.erase(std::string(def.key));
    }
    else {
      config_[std::string(def.key)] = std::move(value);
    }
  }

  save_config();
}

GameOptions ConfigManager::load_options() const {
  GameOptions opts;

  for (const auto& def : option_defs) {
    auto it = config_.find(std::string(def.key));
    if (it != config_.end()) {
      def.set(opts, it->second);
    }
  }

  return opts;
}

std::string ConfigManager::active_game_path() const {
  return value("game_path_english");
}
