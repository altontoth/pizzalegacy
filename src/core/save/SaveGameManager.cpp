/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/save/SaveGameManager.h"

#include <algorithm>
#include <array>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

#include <cstring>

#include <zlib.h>
// zlib.h defines FAR as a macro for legacy DOS far-pointer compat, which
// conflicts with our ZoomLevel::FAR
#undef FAR

#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/MayorOffice.h"
#include "core/city/Property.h"
#include "core/events/FullscreenMessageManager.h"
#include "core/mission/MissionTarget.h"
#include "core/restaurant/Restaurant.h"
#include "core/save/SaveGameCharacter.h"
#include "core/save/SaveGameCity.h"
#include "core/save/SaveGameFood.h"
#include "core/save/SaveGameRestaurant.h"
#include "core/state/GameOptions.h"
#include "core/state/GameState.h"
#include "core/state/TurnOrchestrator.h"
#include "core/time/GameTime.h"
#include "core/util/GameContext.h"
#include "engine/Endian.h"
#include "engine/Logger.h"
#include "engine/Random.h"

namespace save {

namespace fs = std::filesystem;

constexpr uint64_t MAX_SAVE_SIZE = 256ULL * 1024 * 1024; // 256 MB sanity cap

// Save file layout: SaveHeader, then (version >= 2) a uint32 length followed
// by that many bytes of uncompressed metadata JSON, then the zlib payload.
struct SaveHeader {
  char magic[6];    // "PLSAVE"
  uint16_t version; // little-endian
  uint64_t size;    // uncompressed payload size, little-endian
};
static_assert(sizeof(SaveHeader) == 16);

constexpr uint16_t SAVE_HEADER_VERSION = 2;
constexpr uint16_t HIGH_SCORE_HEADER_VERSION = 1;
constexpr uint32_t MAX_METADATA_SIZE = 1024 * 1024;

namespace {

// Extract the load-menu metadata fields from a full save document.
json metadata_from_save_json(const json& j) {
  json meta;
  meta["saved_at"] = j.value("saved_at", "Unknown");
  if (j.contains("time") && j["time"].contains("display")) {
    meta["game_date"] = j["time"]["display"];
  }
  json names = json::array();
  if (j.contains("players")) {
    for (const auto& p : j["players"]) {
      names.push_back(p.is_null() ? "" : p.value("name", ""));
    }
  }
  meta["player_names"] = names;
  meta["active_player"] = j.value("active_player", -1);
  meta["active_city"] = j.value("active_city", -1);
  return meta;
}

} // namespace

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
    std::string json_str = j.dump(-1);

    if (json_str.size() > MAX_SAVE_SIZE ||
        json_str.size() > std::numeric_limits<uLong>::max()) {
      Logger::warn("Save data too large ({} bytes)", json_str.size());
      return false;
    }

    const std::string meta_str = metadata_from_save_json(j).dump(-1);
    if (meta_str.size() > MAX_METADATA_SIZE) {
      Logger::warn("Save metadata too large ({} bytes)", meta_str.size());
      return false;
    }

    const size_t payload_offset =
        sizeof(SaveHeader) + sizeof(uint32_t) + meta_str.size();

    uLongf bound = compressBound(static_cast<uLong>(json_str.size()));
    std::vector<Bytef> out(payload_offset + bound);
    uLongf compressed_size = bound;
    int zret =
        compress2(out.data() + payload_offset, &compressed_size,
                  reinterpret_cast<const Bytef*>(json_str.data()),
                  static_cast<uLong>(json_str.size()), Z_DEFAULT_COMPRESSION);
    if (zret != Z_OK) {
      Logger::warn("Compression failed: {}", zret);
      return false;
    }

    SaveHeader hdr;
    std::memcpy(hdr.magic, "PLSAVE", 6);
    hdr.version = endian::to_le16(SAVE_HEADER_VERSION);
    hdr.size = endian::to_le64(static_cast<uint64_t>(json_str.size()));
    std::memcpy(out.data(), &hdr, sizeof(hdr));

    const uint32_t meta_len =
        endian::to_le32(static_cast<uint32_t>(meta_str.size()));
    std::memcpy(out.data() + sizeof(SaveHeader), &meta_len, sizeof(meta_len));
    std::memcpy(out.data() + sizeof(SaveHeader) + sizeof(uint32_t),
                meta_str.data(), meta_str.size());

    std::string full_path = get_full_path(filename);
    std::string tmp_path = full_path + ".tmp";

    {
      std::ofstream file(tmp_path, std::ios::binary);
      if (!file.is_open()) {
        Logger::warn("Failed to open temp file for writing: {}", tmp_path);
        return false;
      }
      file.write(
          reinterpret_cast<const char*>(out.data()),
          static_cast<std::streamsize>(payload_offset + compressed_size));
    }

    std::error_code ec;
    fs::rename(tmp_path, full_path, ec);
    if (ec) {
      Logger::warn("Failed to rename temp save to {}: {}", full_path,
                   ec.message());
      fs::remove(tmp_path, ec);
      return false;
    }

    Logger::info("Save completed successfully");
    return true;
  }
  catch (const std::exception& e) {
    Logger::warn("Save failed: {}", e.what());
    return false;
  }
}

namespace {

std::string autosave_slot_filename(int slot) {
  return "AUTOSAV" + std::to_string(slot) + ".plsave";
}

std::string next_autosave_filename(int slot_count) {
  fs::path saves_dir = SaveGameManager::get_saves_directory();

  // Fill slots in order before overwriting any
  for (int i = 1; i <= slot_count; ++i) {
    std::error_code ec;
    if (!fs::exists(saves_dir / autosave_slot_filename(i), ec)) {
      return autosave_slot_filename(i);
    }
  }

  // All slots filled; overwrite the one with the oldest mtime
  int oldest_slot = 1;
  fs::file_time_type oldest_time = fs::file_time_type::max();
  for (int i = 1; i <= slot_count; ++i) {
    std::error_code ec;
    auto mtime = fs::last_write_time(saves_dir / autosave_slot_filename(i), ec);
    if (!ec && mtime < oldest_time) {
      oldest_time = mtime;
      oldest_slot = i;
    }
  }
  return autosave_slot_filename(oldest_slot);
}

} // namespace

bool SaveGameManager::autosave(const GameState& state,
                               const GameContext& context) {
  const int slot_count = std::clamp(state.options().autosave_count,
                                    GameOptions::AUTOSAVE_COUNT_MIN,
                                    GameOptions::AUTOSAVE_COUNT_MAX);
  std::string filename = next_autosave_filename(slot_count);
  Logger::info("Autosaving to {}", filename);
  bool ok = save(state, context, filename);
  if (!ok) {
    Logger::warn("Autosave failed");
  }
  return ok;
}

std::optional<json>
SaveGameManager::read_save_json(const std::string& filename) {
  try {
    std::string full_path = get_full_path(filename);
    std::ifstream file(full_path, std::ios::binary);
    if (!file.is_open()) {
      return std::nullopt;
    }

    file.seekg(0, std::ios::end);
    auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (!file.good() || file_size <= 0) {
      Logger::warn("Save file is empty or unreadable: {}", filename);
      return std::nullopt;
    }

    std::vector<unsigned char> compressed(static_cast<size_t>(file_size));
    if (!file.read(reinterpret_cast<char*>(compressed.data()), file_size)) {
      Logger::warn("Failed to read save file: {}", filename);
      return std::nullopt;
    }

    if (compressed.size() < sizeof(SaveHeader)) {
      Logger::warn("Save file too small (no header): {}", filename);
      return std::nullopt;
    }

    SaveHeader hdr;
    std::memcpy(&hdr, compressed.data(), sizeof(hdr));
    if (std::memcmp(hdr.magic, "PLSAVE", 6) != 0) {
      Logger::warn("Save file has invalid magic - incompatible format: {}",
                   filename);
      return std::nullopt;
    }

    uint64_t decompressed_size = endian::from_le64(hdr.size);

    if (decompressed_size == 0 || decompressed_size > MAX_SAVE_SIZE) {
      Logger::warn("Save file has invalid decompressed size ({}): {}",
                   decompressed_size, filename);
      return std::nullopt;
    }

    size_t payload_offset = sizeof(SaveHeader);
    if (endian::from_le16(hdr.version) >= 2) {
      if (compressed.size() < payload_offset + sizeof(uint32_t)) {
        Logger::warn("Save file truncated (no metadata length): {}", filename);
        return std::nullopt;
      }
      uint32_t meta_len = 0;
      std::memcpy(&meta_len, compressed.data() + payload_offset,
                  sizeof(meta_len));
      meta_len = endian::from_le32(meta_len);
      payload_offset += sizeof(uint32_t) + meta_len;
      if (meta_len > MAX_METADATA_SIZE || compressed.size() < payload_offset) {
        Logger::warn("Save file has invalid metadata block: {}", filename);
        return std::nullopt;
      }
    }

    std::string json_str(static_cast<size_t>(decompressed_size), '\0');
    auto dest_len = static_cast<uLongf>(decompressed_size);
    int zret =
        uncompress(reinterpret_cast<Bytef*>(json_str.data()), &dest_len,
                   compressed.data() + payload_offset,
                   static_cast<uLong>(compressed.size() - payload_offset));
    if (zret != Z_OK) {
      Logger::warn("Decompression failed for {}: {}", filename, zret);
      return std::nullopt;
    }
    if (dest_len != static_cast<uLongf>(decompressed_size)) {
      Logger::warn("Decompressed size mismatch for {}: expected {}, got {}",
                   filename, decompressed_size, dest_len);
      return std::nullopt;
    }

    return json::parse(json_str);
  }
  catch (...) {
    return std::nullopt;
  }
}

namespace {

// Read the uncompressed metadata block of a version >= 2 save.
std::optional<json> read_metadata_block(const std::string& full_path) {
  std::ifstream file(full_path, std::ios::binary);
  if (!file.is_open()) {
    return std::nullopt;
  }

  SaveHeader hdr;
  if (!file.read(reinterpret_cast<char*>(&hdr), sizeof(hdr))) {
    return std::nullopt;
  }
  if (std::memcmp(hdr.magic, "PLSAVE", 6) != 0 ||
      endian::from_le16(hdr.version) < 2) {
    return std::nullopt;
  }

  uint32_t meta_len = 0;
  if (!file.read(reinterpret_cast<char*>(&meta_len), sizeof(meta_len))) {
    return std::nullopt;
  }
  meta_len = endian::from_le32(meta_len);
  if (meta_len == 0 || meta_len > MAX_METADATA_SIZE) {
    return std::nullopt;
  }

  std::string meta_str(meta_len, '\0');
  if (!file.read(meta_str.data(), meta_len)) {
    return std::nullopt;
  }
  return json::parse(meta_str);
}

} // namespace

std::optional<SaveMetadata>
SaveGameManager::read_save_metadata(const std::string& filename) {
  std::optional<json> maybe_meta;
  try {
    maybe_meta = read_metadata_block(get_full_path(filename));
  }
  catch (...) {
    return std::nullopt;
  }

  if (!maybe_meta) {
    // Version 1 save: the metadata lives only in the full document.
    auto maybe_j = read_save_json(filename);
    if (!maybe_j) {
      return std::nullopt;
    }
    maybe_meta = metadata_from_save_json(*maybe_j);
  }
  const json& m = *maybe_meta;

  SaveMetadata meta;

  meta.display_name = filename;
  if (meta.display_name.ends_with(".plsave")) {
    meta.display_name =
        meta.display_name.substr(0, meta.display_name.size() - 7);
  }

  meta.saved_at = m.value("saved_at", "Unknown");
  meta.game_date = m.value("game_date", "");

  if (m.contains("player_names")) {
    const auto& names = m["player_names"];
    for (size_t i = 0;
         i < names.size() && i < static_cast<size_t>(NUMBER_OF_PLAYERS); ++i) {
      meta.player_names[i] = names[i].get<std::string>();
    }
  }

  meta.active_player = m.value("active_player", -1);
  meta.active_city = m.value("active_city", -1);

  return meta;
}

bool SaveGameManager::load(GameState& state, GameContext& context,
                           const std::string& filename) {
  try {
    Logger::info("Loading game from {}", filename);

    auto maybe_j = read_save_json(filename);
    if (!maybe_j) {
      Logger::warn("Failed to read save file: {}", filename);
      return false;
    }
    const json& j = *maybe_j;

    int version = j.value("version", -1);
    if (version != SAVE_FORMAT_VERSION) {
      Logger::warn("Save file version mismatch: expected {}, got {}",
                   SAVE_FORMAT_VERSION, version);
      return false;
    }

    json_to_state(j, state, context);
    state.finalize_initialization(context);

    // Land in the city view for the loaded city
    state.ui().pop_to_root();
    state.ui().replace_top<CityViewState>(state.active_city_id());

    Logger::info("Load completed successfully");
    return true;
  }
  catch (const std::exception& e) {
    Logger::warn("Load failed: {}", e.what());
    return false;
  }
}

bool SaveGameManager::is_valid_save(const std::string& filename) {
  auto maybe_j = read_save_json(filename);
  if (!maybe_j) {
    return false;
  }
  const json& j = *maybe_j;
  return j.contains("version") && j.contains("time") &&
         j.contains("active_player");
}

int SaveGameManager::get_save_version(const std::string& filename) {
  auto maybe_j = read_save_json(filename);
  if (!maybe_j) {
    return -1;
  }
  return maybe_j->value("version", -1);
}

namespace {

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

} // namespace

json SaveGameManager::state_to_json(const GameState& state,
                                    const GameContext& /*context*/) {
  json j;

  // Save format version
  j["version"] = SAVE_FORMAT_VERSION;

  // Save RNG state so a loaded game resumes with the same random sequence
  j["rng_state"] = engine::random::get_state();

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
  j["city_start_of_day_ran"] = state.turn().city_start_of_day_flags();
  j["restaurant_day_ran"] = state.turn().restaurant_day_flags();

  if (state.active_restaurant()) {
    j["active_restaurant"] = state.active_restaurant()->id().value;
  }
  else {
    j["active_restaurant"] = nullptr;
  }

  // Save difficulty
  j["difficulty_level"] = state.difficulty_level();
  j["game_mode"] = static_cast<int>(state.game_mode());

  // Save the mission target CV (full dates, so loaded games never depend
  // on the .mis file or its year encoding again)
  if (state.mission_target().has_value()) {
    const auto& target = *state.mission_target();
    json mission_array = json::array();
    for (const auto& milestone : target.milestones) {
      mission_array.push_back({{"id", static_cast<int>(milestone.message_id)},
                               {"y", milestone.date.year()},
                               {"m", milestone.date.month()},
                               {"d", milestone.date.day()}});
    }
    j["mission_target"] = {{"source_name", target.source_name},
                           {"milestones", mission_array}};
  }

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
  auto visited = state.initialized_cities();
  for (City* city : visited) {
    cities_array.push_back(city_to_json(*city));
  }
  j["cities"] = cities_array;

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

  // Save deferred staff events (queued at EOD, delivered at 10am next day)
  {
    json staff_array = json::array();
    for (const auto& ev : state.turn().deferred_staff_events()) {
      staff_array.push_back({{"restaurant_id", ev.restaurant_id.value},
                             {"type", static_cast<int>(ev.event.type)},
                             {"job_role", static_cast<int>(ev.event.job_role)},
                             {"slot_index", ev.event.slot_index}});
    }
    j["deferred_staff_events"] = staff_array;
  }

  // Save deferred mayor events (queued at EOD, delivered at 10am next day)
  {
    json insp_array = json::array();
    for (const auto& ev : state.turn().deferred_mayor_inspections()) {
      insp_array.push_back({{"restaurant_id", ev.restaurant_id.value},
                            {"is_first", ev.is_first}});
    }
    j["deferred_mayor_inspections"] = insp_array;

    json award_array = json::array();
    for (const auto& ev : state.turn().deferred_mayor_awards()) {
      json entry = {{"type", static_cast<int>(ev.type)},
                    {"is_ceremony", ev.is_ceremony}};
      if (ev.winner_player_id.has_value()) {
        entry["winner_player_id"] = *ev.winner_player_id;
      }
      else {
        entry["winner_player_id"] = nullptr;
      }
      award_array.push_back(entry);
    }
    j["deferred_mayor_awards"] = award_array;
  }

  // Save deferred protection racket offers (rolled at EOD, delivered on the
  // owning player's next turn)
  {
    json racket_array = json::array();
    for (const auto& offer : state.turn().deferred_racket_offers()) {
      racket_array.push_back({{"player_id", offer.player_id.value},
                              {"restaurant_id", offer.restaurant_id.value},
                              {"first_contact", offer.first_contact}});
    }
    j["deferred_racket_offers"] = racket_array;
  }

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
  state.reset_for_load();

  // Restore RNG state so the loaded game continues with the same random
  // sequence
  if (j.contains("rng_state")) {
    engine::random::set_state(j["rng_state"].get<uint32_t>());
  }

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
  if (j.contains("mission_target")) {
    const auto& mt = j["mission_target"];
    core::mission::MissionTarget target;
    target.source_name = mt.value("source_name", std::string{});
    for (const auto& milestone : mt["milestones"]) {
      target.milestones.push_back(
          {GameDate{milestone["y"].get<int>(), milestone["m"].get<int>(),
                    milestone["d"].get<int>()},
           static_cast<core::events::LeventId>(milestone["id"].get<int>())});
    }
    state.mission_target(std::move(target));
  }
  if (j.contains("difficulty_level")) {
    state.difficulty_level(j["difficulty_level"].get<int>());
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
      auto restaurant = json_to_restaurant(rest_json, *property);

      // Register the restaurant ID with the property
      property->restaurant_id(restaurant->id());

      // Add to game state
      state.add_restaurant(std::move(restaurant));
    }
  }

  // Making a city active runs its start of day unless it already ran, so we
  // have to set these flags first.
  if (j.contains("city_start_of_day_ran")) {
    state.turn().city_start_of_day_flags(
        j["city_start_of_day_ran"].get<TurnOrchestrator::CityDayFlags>());
  }
  if (j.contains("restaurant_day_ran")) {
    state.turn().restaurant_day_flags(
        j["restaurant_day_ran"].get<TurnOrchestrator::RestaurantDayFlags>());
  }
  if (j.contains("active_city")) {
    core::CityId cid{j["active_city"].get<int>()};
    state.active_city(cid);
    Logger::info("Set active city to {}", cid.value);
  }

  // Set active restaurant after restaurants are loaded
  if (j.contains("active_restaurant") && !j["active_restaurant"].is_null()) {
    core::RestaurantId rid{j["active_restaurant"].get<int>()};
    state.active_restaurant(rid);
  }

  // Restore deferred staff events
  if (j.contains("deferred_staff_events")) {
    for (const auto& ev : j["deferred_staff_events"]) {
      core::restaurant::StaffEventData data;
      data.type = static_cast<core::restaurant::StaffEventData::Type>(
          ev["type"].get<int>());
      data.job_role =
          static_cast<core::character::JobRole>(ev["job_role"].get<int>());
      data.slot_index = ev["slot_index"].get<int>();
      state.turn().restore_deferred_staff_event(
          data, core::RestaurantId{ev["restaurant_id"].get<int>()});
    }
  }

  // Restore deferred mayor events
  if (j.contains("deferred_mayor_inspections")) {
    for (const auto& ev : j["deferred_mayor_inspections"]) {
      core::RestaurantId rid{ev["restaurant_id"].get<int>()};
      bool is_first = ev["is_first"].get<bool>();
      state.turn().restore_deferred_mayor_inspection(rid, is_first);
    }
  }
  if (j.contains("deferred_mayor_awards")) {
    for (const auto& ev : j["deferred_mayor_awards"]) {
      MayorAwardEvent award;
      award.type = static_cast<MayorAwardType>(ev["type"].get<int>());
      award.is_ceremony =
          ev.contains("is_ceremony") && ev["is_ceremony"].get<bool>();
      if (ev.contains("winner_player_id") &&
          !ev["winner_player_id"].is_null()) {
        award.winner_player_id = ev["winner_player_id"].get<int>();
      }
      state.turn().queue_mayor_award(award);
    }
  }

  // Restore deferred protection racket offers
  if (j.contains("deferred_racket_offers")) {
    for (const auto& ev : j["deferred_racket_offers"]) {
      state.turn().restore_deferred_racket_offer(
          {.player_id = core::PlayerId{ev["player_id"].get<int>()},
           .restaurant_id = core::RestaurantId{ev["restaurant_id"].get<int>()},
           .first_contact = ev["first_contact"].get<bool>()});
    }
  }

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

static constexpr std::array<char, 6> HIGH_SCORE_MAGIC = {'P', 'L', 'H',
                                                         'I', 'G', 'H'};
static constexpr std::string_view HIGH_SCORE_FILENAME = "highscore.dat";

std::vector<HighScoreEntry> SaveGameManager::load_high_scores() {
  try {
    std::string full_path = get_full_path(std::string(HIGH_SCORE_FILENAME));
    std::ifstream file(full_path, std::ios::binary);
    if (!file.is_open()) {
      return {};
    }

    file.seekg(0, std::ios::end);
    auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (!file.good() ||
        file_size <= static_cast<std::streampos>(sizeof(SaveHeader))) {
      return {};
    }

    std::vector<unsigned char> compressed(static_cast<size_t>(file_size));
    if (!file.read(reinterpret_cast<char*>(compressed.data()), file_size)) {
      return {};
    }

    SaveHeader hdr;
    std::memcpy(&hdr, compressed.data(), sizeof(hdr));
    if (std::memcmp(hdr.magic, HIGH_SCORE_MAGIC.data(), 6) != 0) {
      Logger::warn("High score file has invalid magic");
      return {};
    }

    uint64_t decompressed_size = endian::from_le64(hdr.size);
    if (decompressed_size == 0 || decompressed_size > MAX_SAVE_SIZE) {
      return {};
    }

    std::string json_str(static_cast<size_t>(decompressed_size), '\0');
    auto dest_len = static_cast<uLongf>(decompressed_size);
    int zret =
        uncompress(reinterpret_cast<Bytef*>(json_str.data()), &dest_len,
                   compressed.data() + sizeof(SaveHeader),
                   static_cast<uLong>(compressed.size() - sizeof(SaveHeader)));
    if (zret != Z_OK) {
      return {};
    }

    auto j = json::parse(json_str);
    std::vector<HighScoreEntry> entries;
    for (const auto& e : j.value("entries", json::array())) {
      entries.push_back({
          .name = e.value("name", ""),
          .score = e.value("score", 0),
      });
    }
    return entries;
  }
  catch (...) {
    return {};
  }
}

bool SaveGameManager::add_high_score(HighScoreEntry entry) {
  try {
    auto entries = load_high_scores();
    entries.push_back(std::move(entry));

    std::sort(entries.begin(), entries.end(),
              [](const HighScoreEntry& a, const HighScoreEntry& b) {
                return a.score > b.score;
              });
    if (static_cast<int>(entries.size()) > HIGH_SCORE_MAX_ENTRIES) {
      entries.resize(HIGH_SCORE_MAX_ENTRIES);
    }

    json j;
    j["version"] = 1;
    j["entries"] = json::array();
    for (const auto& e : entries) {
      j["entries"].push_back({{"name", e.name}, {"score", e.score}});
    }
    std::string json_str = j.dump(-1);

    uLongf bound = compressBound(static_cast<uLong>(json_str.size()));
    std::vector<Bytef> compressed(sizeof(SaveHeader) + bound);
    uLongf compressed_size = bound;
    int zret =
        compress2(compressed.data() + sizeof(SaveHeader), &compressed_size,
                  reinterpret_cast<const Bytef*>(json_str.data()),
                  static_cast<uLong>(json_str.size()), Z_DEFAULT_COMPRESSION);
    if (zret != Z_OK) {
      return false;
    }

    SaveHeader hdr;
    std::memcpy(hdr.magic, HIGH_SCORE_MAGIC.data(), 6);
    hdr.version = endian::to_le16(HIGH_SCORE_HEADER_VERSION);
    hdr.size = endian::to_le64(static_cast<uint64_t>(json_str.size()));
    std::memcpy(compressed.data(), &hdr, sizeof(hdr));

    std::string full_path = get_full_path(std::string(HIGH_SCORE_FILENAME));
    std::ofstream file(full_path, std::ios::binary);
    if (!file.is_open()) {
      return false;
    }
    file.write(
        reinterpret_cast<const char*>(compressed.data()),
        static_cast<std::streamsize>(sizeof(SaveHeader) + compressed_size));
    return true;
  }
  catch (...) {
    return false;
  }
}

} // namespace save
