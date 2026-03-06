/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>

class GameState;
class GameContext;

namespace save {

using json = nlohmann::json;

// Version of the save format
constexpr int SAVE_FORMAT_VERSION = 1;

class SaveGameManager {
public:
  // Save the current game state to a file.
  static bool save(const GameState& state, const GameContext& context,
                   const std::string& filename);
  static bool load(GameState& state, GameContext& context,
                   const std::string& filename);
  static bool is_valid_save(const std::string& filename);
  static int get_save_version(const std::string& filename);

  static void set_saves_directory(std::string dir);
  static const std::string& get_saves_directory();

private:
  static json state_to_json(const GameState& state, const GameContext& context);
  static void json_to_state(const json& j, GameState& state,
                            GameContext& context);

  static std::string saves_directory_;
  static std::string get_full_path(const std::string& filename);
};

} // namespace save
