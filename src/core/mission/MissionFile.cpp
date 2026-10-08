/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/mission/MissionFile.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>
#include <vector>
#include "core/events/MilestoneId.h"
#include "core/food/PizzaJudging.h"
#include "core/state/GameRules.h"
#include "core/time/GameDate.h"
#include "engine/Logger.h"
#include "engine/PlatformPaths.h"
#include "resources/util/FileUtils.h"

namespace core::mission {

namespace fs = std::filesystem;

namespace {

// Column offsets within the 400-byte file.
constexpr int MIS_EVENT_OFFSET = 0 * MIS_SLOT_COUNT;
constexpr int MIS_DAY_OFFSET = 1 * MIS_SLOT_COUNT;
constexpr int MIS_MONTH_OFFSET = 2 * MIS_SLOT_COUNT;
constexpr int MIS_YEAR_OFFSET = 3 * MIS_SLOT_COUNT;

// The rules trailer sits behind the 400 bytes the original reads.
constexpr std::array<uint8_t, 4> trailer_magic{'P', 'L', 'R', 'S'};
constexpr uint8_t trailer_version = 1;
constexpr int trailer_header_size = trailer_magic.size() + 2;

// The trailer: the something-changed flag, then the rules in a fixed order (so
// we can load missions from newer versions by just ignoring the bytes beyond
// what we know about).
std::vector<uint8_t> encode_rules(const GameRules& rules, bool rules_changed) {
  return {
      static_cast<uint8_t>(rules_changed),
      static_cast<uint8_t>(rules.fix_legacy_bugs),
      static_cast<uint8_t>(rules.mafia_shakedowns_target_all_restaurants),
      static_cast<uint8_t>(rules.demand_fulfillment_popularity),
      static_cast<uint8_t>(rules.win_condition),
      static_cast<uint8_t>(rules.pizza_judging),
      static_cast<uint8_t>(rules.police),
      static_cast<uint8_t>(rules.suppliers_remember_missed_payments),
      static_cast<uint8_t>(rules.cook_book),
  };
}

void decode_rules(const std::vector<uint8_t>& bytes, MissionTarget& target) {
  GameRules rules;
  auto decode_bool = [&](size_t index, bool& value) {
    if (index < bytes.size()) {
      value = bytes[index] != 0;
    }
  };
  decode_bool(0, target.rules_changed);
  decode_bool(1, rules.fix_legacy_bugs);
  decode_bool(2, rules.mafia_shakedowns_target_all_restaurants);
  decode_bool(3, rules.demand_fulfillment_popularity);
  if (bytes.size() > 4 &&
      bytes[4] <= static_cast<uint8_t>(WinCondition::AllCities)) {
    rules.win_condition = static_cast<WinCondition>(bytes[4]);
  }
  if (bytes.size() > 5 &&
      bytes[5] <= static_cast<uint8_t>(PizzaJudging::Relaxed)) {
    rules.pizza_judging = static_cast<PizzaJudging>(bytes[5]);
  }
  decode_bool(6, rules.police);
  decode_bool(7, rules.suppliers_remember_missed_payments);
  decode_bool(8, rules.cook_book);
  target.rules = rules;
}

// Set the target's rules from the trailer; they stay unset when the file
// has no trailer or one we don't recognize.
void read_trailer(const std::vector<uint8_t>& file, const fs::path& path,
                  MissionTarget& target) {
  if (file.size() <= MIS_FILE_SIZE) {
    return;
  }
  const auto trailer = file.begin() + MIS_FILE_SIZE;
  const auto available = static_cast<int>(file.size()) - MIS_FILE_SIZE;
  if (available < trailer_header_size ||
      !std::equal(trailer_magic.begin(), trailer_magic.end(), trailer)) {
    Logger::warn("Mission file has {} bytes past the mission that are not a "
                 "rules trailer: {}",
                 available, path.string());
    return;
  }
  const uint8_t version = trailer[trailer_magic.size()];
  const int length = trailer[trailer_magic.size() + 1];
  if (version != trailer_version || available < trailer_header_size + length) {
    Logger::warn("Mission file rules trailer is version {} with {} bytes, "
                 "which this build cannot read: {}",
                 version, length, path.string());
    return;
  }
  const auto payload = trailer + trailer_header_size;
  decode_rules(std::vector<uint8_t>(payload, payload + length), target);
}

// Event ids that exist as byte values but are not real milestones:
// 28/29 are UI header lines, 93 is an empty line, 98/99 are unused slots.
bool is_valid_milestone_event(int event_id) {
  if (event_id < 1 || event_id > core::events::max_milestone_id) {
    return false;
  }
  return event_id != 28 && event_id != 29 && event_id != 93 && event_id != 98 &&
         event_id != 99;
}

bool has_mis_extension(const fs::path& p) {
  std::string ext = p.extension().string();
  std::ranges::transform(ext, ext.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return ext == ".mis";
}

void append_missions_from_dir(const fs::path& dir,
                              const std::string& display_prefix,
                              std::vector<MissionListEntry>& out) {
  if (dir.empty() || !fs::exists(dir) || !fs::is_directory(dir)) {
    return;
  }
  std::vector<fs::path> paths;
  for (const auto& entry : fs::directory_iterator(dir)) {
    if (entry.is_regular_file() && has_mis_extension(entry.path())) {
      paths.push_back(entry.path());
    }
  }
  std::ranges::sort(paths);
  for (auto& p : paths) {
    out.push_back({display_prefix + p.filename().string(), std::move(p)});
  }
}

} // anonymous namespace

std::optional<MissionTarget> read_mission_file(const fs::path& path) {
  std::ifstream is(path, std::ios::binary);
  if (!is) {
    Logger::warn("Mission file could not be opened: {}", path.string());
    return std::nullopt;
  }

  const std::vector<uint8_t> buf(std::istreambuf_iterator<char>(is), {});
  if (buf.size() < MIS_FILE_SIZE) {
    Logger::warn("Mission file is shorter than {} bytes: {}", MIS_FILE_SIZE,
                 path.string());
    return std::nullopt;
  }

  MissionTarget target;
  target.source_name = path.filename().string();
  read_trailer(buf, path, target);

  for (int slot = 0; slot < MIS_SLOT_COUNT; ++slot) {
    const uint8_t year_byte = buf[MIS_YEAR_OFFSET + slot];
    if (year_byte == 0) {
      continue; // empty slot
    }
    const int event_id = buf[MIS_EVENT_OFFSET + slot];
    const int day = buf[MIS_DAY_OFFSET + slot];
    const int month = buf[MIS_MONTH_OFFSET + slot];
    const int year = MIS_YEAR_BIAS + year_byte;

    if (!is_valid_milestone_event(event_id) || month < 1 || month > 12 ||
        day < 1 || day > 31) {
      Logger::warn("Skipping invalid mission slot {} in {} "
                   "(event {}, date {}-{}-{})",
                   slot, path.string(), event_id, day, month, year);
      continue;
    }

    target.milestones.push_back(
        {GameDate{year, month, day},
         static_cast<core::events::MilestoneId>(event_id)});
  }

  // A mission with no milestones is pointless, so we treat it like a failed
  // load, even if the file is technically correct. You can't save a mission
  // without first arriving in a city anyway, so having zero milestones at save
  // time should be impossible anyway.
  if (target.milestones.empty()) {
    Logger::warn("Mission file has no valid milestones: {}", path.string());
    return std::nullopt;
  }

  return target;
}

void shift_mission_to_game_start(MissionTarget& target) {
  if (target.milestones.empty()) {
    return;
  }

  int min_year = target.milestones.front().date.year();
  for (const auto& milestone : target.milestones) {
    min_year = std::min(min_year, milestone.date.year());
  }
  const int delta = game_start_date.year() - min_year;
  if (delta <= 0) {
    return;
  }

  Logger::info("Mission {} starts in {}; shifting all dates by {} year(s)",
               target.source_name, min_year, delta);
  for (auto& milestone : target.milestones) {
    milestone.date = milestone.date.add_years(delta);
  }
}

bool write_mission_file(const fs::path& path,
                        const std::vector<TargetMilestone>& cv_entries,
                        const GameRules& rules, bool rules_changed) {
  if (cv_entries.size() > MIS_SLOT_COUNT) {
    Logger::warn("Career has {} entries; only the first {} fit in a mission",
                 cv_entries.size(), MIS_SLOT_COUNT);
  }
  const int count =
      std::min(static_cast<int>(cv_entries.size()), MIS_SLOT_COUNT);

  std::array<uint8_t, MIS_FILE_SIZE> buf{};
  for (int slot = 0; slot < count; ++slot) {
    const auto& entry = cv_entries[slot];
    buf[MIS_EVENT_OFFSET + slot] = static_cast<uint8_t>(entry.message_id);
    buf[MIS_DAY_OFFSET + slot] = static_cast<uint8_t>(entry.date.day());
    buf[MIS_MONTH_OFFSET + slot] = static_cast<uint8_t>(entry.date.month());
    buf[MIS_YEAR_OFFSET + slot] =
        static_cast<uint8_t>(entry.date.year() - MIS_YEAR_BIAS);
  }

  std::ofstream os(path, std::ios::binary | std::ios::trunc);
  if (!os) {
    Logger::warn("Could not open mission file for writing: {}", path.string());
    return false;
  }
  os.write(reinterpret_cast<const char*>(buf.data()), buf.size());

  const std::vector<uint8_t> payload = encode_rules(rules, rules_changed);
  std::vector<uint8_t> trailer(trailer_magic.begin(), trailer_magic.end());
  trailer.push_back(trailer_version);
  trailer.push_back(static_cast<uint8_t>(payload.size()));
  trailer.insert(trailer.end(), payload.begin(), payload.end());
  os.write(reinterpret_cast<const char*>(trailer.data()), trailer.size());
  return os.good();
}

std::vector<MissionListEntry> list_available_missions() {
  std::vector<MissionListEntry> missions;

  // Built-in missions from the original game data (m/0.mis, m/1.mis, ...).
  if (auto m_dir = fileutils::find_file_path("m")) {
    append_missions_from_dir(*m_dir, "m/", missions);
  }

  // Player-saved missions.
  append_missions_from_dir(platform_paths::missions_dir(), "", missions);

  return missions;
}

std::vector<fileutils::FileEntry> list_player_mission_files() {
  std::vector<MissionListEntry> missions;
  append_missions_from_dir(platform_paths::missions_dir(), "", missions);

  std::vector<fileutils::FileEntry> files;
  files.reserve(missions.size());
  for (const auto& mission : missions) {
    std::error_code ec;
    auto mtime = fs::last_write_time(mission.path, ec);
    files.push_back({mission.display_name, mtime});
  }
  return files;
}

std::optional<fs::path>
save_career_mission(const std::string& name,
                    const std::vector<TargetMilestone>& cv_entries,
                    const GameRules& rules, bool rules_changed) {
  const fs::path dir = platform_paths::missions_dir();
  std::error_code ec;
  fs::create_directories(dir, ec);

  fs::path path = dir / (name + ".mis");
  if (!write_mission_file(path, cv_entries, rules, rules_changed)) {
    return std::nullopt;
  }
  return path;
}

} // namespace core::mission
