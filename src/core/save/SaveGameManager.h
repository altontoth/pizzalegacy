/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "core/util/Constants.h"

class GameState;
class GameContext;

namespace save {

using json = nlohmann::json;

// Version of the save format
constexpr int SAVE_FORMAT_VERSION = 3;

// The oldest version a save can have and still be loaded.
constexpr int OLDEST_LOADABLE_SAVE_VERSION = 1;

struct SaveMetadata {
  std::string display_name;
  std::string saved_at;
  std::string game_date;
  std::array<std::string, NUMBER_OF_PLAYERS> player_names;
  int active_player = -1;
  int active_city = -1;
};

// Highscore files are basically the same as save games, zlib compressed json
// with a magic header, so for now I don't think it's worth pulling that out
// into its own class.
struct HighScoreEntry {
  std::string name;
  int score;
};

constexpr int HIGH_SCORE_MAX_ENTRIES = 8;

struct StateFingerprint {
  std::string hash; // 16 hex digits
  json document;    // the state we hashed, everything but the save timestamp
};

class SaveGameManager {
public:
  static bool save(const GameState& state, const GameContext& context,
                   const std::string& filename);
  static bool autosave(const GameState& state, const GameContext& context);
  static bool load(GameState& state, GameContext& context,
                   const std::string& filename);
  static bool is_valid_save(const std::string& filename);
  static int get_save_version(const std::string& filename);

  // Read metadata from a save file without loading the full game state.
  static std::optional<SaveMetadata>
  read_save_metadata(const std::string& filename);

  static void set_saves_directory(std::string dir);
  static const std::string& get_saves_directory();

  static std::vector<HighScoreEntry> load_high_scores();
  static bool add_high_score(HighScoreEntry entry);

  static StateFingerprint fingerprint(const GameState& state,
                                      const GameContext& context);

private:
  static std::optional<json> read_save_json(const std::string& filename);

  static json state_to_json(const GameState& state, const GameContext& context);
  static void json_to_state(const json& j, GameState& state,
                            GameContext& context);

  static std::string saves_directory_;
  static std::string get_full_path(const std::string& filename);
};

} // namespace save
