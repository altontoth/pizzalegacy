/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/save/SaveGameManager.h"

#include <ctime>
#include <filesystem>
#include <fstream>

#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/Property.h"
#include "core/events/FullscreenMessageManager.h"
#include "core/restaurant/Restaurant.h"
#include "core/save/SaveGameCharacter.h"
#include "core/save/SaveGameCity.h"
#include "core/save/SaveGameFood.h"
#include "core/save/SaveGameRestaurant.h"
#include "core/state/GameState.h"
#include "core/time/GameTime.h"
#include "core/util/GameContext.h"
#include "engine/Logger.h"

namespace save {

namespace fs = std::filesystem;

std::string SaveGameManager::saves_directory_;

void SaveGameManager::set_saves_directory(std::string dir) {
  saves_directory_ = std::move(dir);
}

const std::string& SaveGameManager::get_saves_directory() {
  if (saves_directory_.empty()) {
    static const std::string fallback = "saves";
    return fallback;
  }
  return saves_directory_;
}

std::string SaveGameManager::get_full_path(const std::string& filename) {
  fs::path saves_dir = get_saves_directory();

  // Create saves directory if it doesn't exist
  if (!fs::exists(saves_dir)) {
    fs::create_directories(saves_dir);
  }

  return (saves_dir / filename).string();
}

bool SaveGameManager::save(const GameState& state, const GameContext& context,
                           const std::string& filename) {
  try {
    Logger::info("Saving game to {}", filename);

    json j = state_to_json(state, context);

    std::string full_path = get_full_path(filename);
    std::ofstream file(full_path);

    if (!file.is_open()) {
      Logger::warn("Failed to open file for writing: {}", full_path);
      return false;
    }

    // Pretty print with 2-space indentation
    file << j.dump(2);
    file.close();

    Logger::info("Save completed successfully");
    return true;
  }
  catch (const std::exception& e) {
    Logger::warn("Save failed: {}", e.what());
    return false;
  }
}

bool SaveGameManager::load(GameState& state, GameContext& context,
                           const std::string& filename) {
  try {
    Logger::info("Loading game from {}", filename);

    std::string full_path = get_full_path(filename);
    std::ifstream file(full_path);

    if (!file.is_open()) {
      Logger::warn("Failed to open file for reading: {}", full_path);
      return false;
    }

    json j;
    file >> j;
    file.close();

    // Check version
    int version = j.value("version", -1);
    if (version != SAVE_FORMAT_VERSION) {
      Logger::warn("Save file version mismatch: expected {}, got {}",
                   SAVE_FORMAT_VERSION, version);
      // For now, reject mismatched versions
      // Later we can add migration logic
      return false;
    }

    json_to_state(j, state, context);

    Logger::info("Load completed successfully");
    return true;
  }
  catch (const std::exception& e) {
    Logger::warn("Load failed: {}", e.what());
    return false;
  }
}

bool SaveGameManager::is_valid_save(const std::string& filename) {
  try {
    std::string full_path = get_full_path(filename);

    if (!fs::exists(full_path)) {
      return false;
    }

    std::ifstream file(full_path);
    if (!file.is_open()) {
      return false;
    }

    json j;
    file >> j;

    // Check if it has required fields
    return j.contains("version") && j.contains("time") &&
           j.contains("active_player");
  }
  catch (...) {
    return false;
  }
}

int SaveGameManager::get_save_version(const std::string& filename) {
  try {
    std::string full_path = get_full_path(filename);
    std::ifstream file(full_path);

    if (!file.is_open()) {
      return -1;
    }

    json j;
    file >> j;

    return j.value("version", -1);
  }
  catch (...) {
    return -1;
  }
}

json time_to_json(const GameTime& time) {
  return {
      {"blocks_since_start", time.time_blocks()},
      {"display", time.to_string()} // Human-readable for debugging
  };
}

GameTime json_to_time(const json& j) {
  GameTime time;
  if (j.contains("blocks_since_start")) {
    time.time_blocks(j["blocks_since_start"].get<int>());
  }
  return time;
}

json SaveGameManager::state_to_json(const GameState& state,
                                    const GameContext& context) {
  json j;

  // Save format version
  j["version"] = SAVE_FORMAT_VERSION;

  // Save timestamp (ctime includes a trailing newline, so we need to trim it)
  std::time_t now = std::time(nullptr);
  std::string timestamp = std::ctime(&now);
  if (!timestamp.empty() && timestamp.back() == '\n') {
    timestamp.pop_back();
  }
  j["saved_at"] = timestamp;

  // Save game time
  j["time"] = time_to_json(state.time());

  // Save active IDs
  j["active_player"] = state.active_player()->id().value;
  j["active_city"] = state.active_city_id().value;

  if (state.active_restaurant()) {
    j["active_restaurant"] = state.active_restaurant()->id().value;
  }
  else {
    j["active_restaurant"] = nullptr;
  }

  // Save difficulty
  j["difficulty_level"] = state.difficulty_level();
  j["game_mode"] = static_cast<int>(state.game_mode());

  // Save customer taste profiles
  {
    json taste_array = json::array();
    for (auto demo : ALL_CONSUMER_TYPES) {
      const auto& tp = state.customer_taste_profiles()[demo];
      taste_array.push_back({{"fat", tp.fat},
                             {"sweet", tp.sweet},
                             {"salty", tp.salty},
                             {"hot", tp.hot},
                             {"spicy", tp.spicy}});
    }
    j["customer_taste_profiles"] = taste_array;
  }

  // Save players
  json players_array = json::array();
  auto players = state.players();
  for (size_t i = 0; i < players.size(); ++i) {
    if (players[i] != nullptr) {
      players_array.push_back(player_to_json(*players[i]));
    }
    else {
      players_array.push_back(nullptr);
    }
  }
  j["players"] = players_array;

  // Save visited cities only
  json cities_array = json::array();
  auto visited = state.visited_cities();
  Logger::info("About to save {} visited cities:", visited.size());
  for (City* city : visited) {
    Logger::info("  - City {}", city->id().value);
    cities_array.push_back(city_to_json(*city));
  }
  j["cities"] = cities_array;
  Logger::info("Saved {} cities to JSON", cities_array.size());

  // Save restaurants
  json restaurants_array = json::array();
  // We need to access the restaurants through GameState methods
  // For now, save all restaurants we can find through players
  for (size_t pid = 0; pid < players.size(); ++pid) {
    if (players[pid] != nullptr) {
      auto player_restaurants =
          state.restaurants_for(core::PlayerId{static_cast<int>(pid)});
      for (auto* restaurant : player_restaurants) {
        if (restaurant) {
          restaurants_array.push_back(restaurant_to_json(*restaurant));
        }
      }
    }
  }
  j["restaurants"] = restaurants_array;

  // Save shown message history with dates (for CV display and deduplication)
  json shown_msgs;
  for (size_t pid = 0; pid < players.size(); ++pid) {
    if (players[pid] != nullptr) {
      auto player_id = core::PlayerId{static_cast<int>(pid)};
      auto entries = state.message_manager().get_cv_entries_for(player_id);
      if (!entries.empty()) {
        json entry_array = json::array();
        for (const auto& entry : entries) {
          entry_array.push_back({{"id", static_cast<int>(entry.message_id)},
                                 {"y", entry.date.year()},
                                 {"m", entry.date.month()},
                                 {"d", entry.date.day()}});
        }
        shown_msgs[std::to_string(pid)] = entry_array;
      }
    }
  }
  j["shown_messages"] = shown_msgs;

  return j;
}

void SaveGameManager::json_to_state(const json& j, GameState& state,
                                    GameContext& context) {
  // Clear all existing restaurants and reset property states before loading
  state.clear_restaurants_for_load();

  // Load game time
  if (j.contains("time")) {
    GameTime loaded_time = json_to_time(j["time"]);
    state.set_time(loaded_time);
  }

  // Load active player ID
  if (j.contains("active_player")) {
    core::PlayerId pid{j["active_player"].get<int>()};
    state.active_player(pid);
  }
  if (j.contains("game_mode")) {
    auto mode{j["game_mode"].get<int>()};
    state.game_mode(static_cast<GameMode>(mode));
  }

  // active_city and active_restaurant will be set after
  // cities/restaurants are loaded (to avoid them being overwritten during city
  // creation)

  // Load customer taste profiles (before players, since pizza loading uses
  // them)
  if (j.contains("customer_taste_profiles")) {
    const auto& taste_array = j["customer_taste_profiles"];
    for (size_t i = 0;
         i < taste_array.size() && i < static_cast<size_t>(ConsumerType::Count);
         ++i) {
      auto demo = static_cast<ConsumerType>(i);
      auto& tp = state.customer_taste_profiles()[demo];
      tp.fat = taste_array[i]["fat"].get<int>();
      tp.sweet = taste_array[i]["sweet"].get<int>();
      tp.salty = taste_array[i]["salty"].get<int>();
      tp.hot = taste_array[i]["hot"].get<int>();
      tp.spicy = taste_array[i]["spicy"].get<int>();
    }
  }

  // Load players
  if (j.contains("players")) {
    std::array<std::unique_ptr<Player>, NUMBER_OF_PLAYERS> players;
    for (size_t i = 0; i < j["players"].size(); ++i) {
      if (!j["players"][i].is_null()) {
        players[i] = json_to_player(j["players"][i], context,
                                    state.customer_taste_profiles());
      }
    }
    state.players(std::move(players));
  }

  // Load cities (must be done BEFORE restaurants to restore property state)
  if (j.contains("cities")) {
    Logger::info("Loading {} cities from save file", j["cities"].size());
    for (const auto& city_json : j["cities"]) {
      core::CityId city_id{city_json["id"].get<int>()};
      Logger::info("Loading city {}", city_id.value);

      // Ensure city exists WITHOUT changing active_city
      state.ensure_city_loaded(city_id);
      City* city = state.city(city_id);

      if (city) {
        json_to_city(city_json, *city);
        Logger::info("Restored city {} with {} properties", city_id.value,
                     city_json["properties"].size());
      }
      else {
        Logger::warn("Failed to create city {}", city_id.value);
      }
    }
  }

  // Set active city after cities are loaded
  if (j.contains("active_city")) {
    core::CityId cid{j["active_city"].get<int>()};
    state.active_city(cid);
    Logger::info("Set active city to {}", cid.value);
  }

  // Load restaurants (after cities so properties are in correct state)
  if (j.contains("restaurants")) {
    for (const auto& rest_json : j["restaurants"]) {
      // Get the property reference
      core::CityId city_id{rest_json["property_city_id"].get<int>()};
      int property_index = rest_json["property_index"].get<int>();

      City* city = state.city(city_id);
      if (!city) {
        Logger::warn("Failed to find city {} for restaurant", city_id.value);
        continue;
      }

      Property* property = city->property_by_index(property_index);
      if (!property) {
        Logger::warn("Failed to find property {} in city {} for restaurant",
                     property_index, city_id.value);
        continue;
      }

      // Reconstruct the restaurant
      auto restaurant = json_to_restaurant(rest_json, property);

      // Register the restaurant ID with the property
      property->restaurant_id(restaurant->id());

      // Add to game state
      state.add_restaurant(std::move(restaurant));
    }
  }

  // Set active restaurant after restaurants are loaded
  if (j.contains("active_restaurant") && !j["active_restaurant"].is_null()) {
    core::RestaurantId rid{j["active_restaurant"].get<int>()};
    state.active_restaurant(rid);
  }

  // Reload article database (needed for newspaper display after load)
  state.article_database().load_articles();

  // Restore shown message history (clear slate first, then load from save)
  if (j.contains("shown_messages")) {
    for (const auto& [pid_str, entries_json] : j["shown_messages"].items()) {
      auto pid = core::PlayerId{std::stoi(pid_str)};
      state.message_manager().clear_player_messages(pid);
      std::vector<core::events::FullscreenMessageManager::CvEntry> entries;
      for (const auto& val : entries_json) {
        auto id = static_cast<core::events::LeventId>(val.at("id").get<int>());
        GameDate date{val.at("y").get<int>(), val.at("m").get<int>(),
                      val.at("d").get<int>()};
        entries.push_back({date, id});
      }
      state.message_manager().restore_scheduled(pid, entries);
    }
  }
}

} // namespace save
