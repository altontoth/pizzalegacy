/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/mission/MissionFile.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <system_error>
#include <utility>
#include "core/events/MilestoneId.h"
#include "core/time/GameDate.h"
#include "engine/Logger.h"
#include "resources/util/FileUtils.h"

namespace core::mission {

namespace fs = std::filesystem;

namespace {

std::string missions_directory_;

// Column offsets within the 400-byte file.
constexpr int MIS_EVENT_OFFSET = 0 * MIS_SLOT_COUNT;
constexpr int MIS_DAY_OFFSET = 1 * MIS_SLOT_COUNT;
constexpr int MIS_MONTH_OFFSET = 2 * MIS_SLOT_COUNT;
constexpr int MIS_YEAR_OFFSET = 3 * MIS_SLOT_COUNT;

// Event ids that exist as byte values but are not real milestones:
// 28/29 are UI header lines, 93 is an empty line, 98/99 are unused slots.
bool is_valid_milestone_event(int event_id) {
  if (event_id < 1 || event_id > 139) {
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

  std::array<uint8_t, MIS_FILE_SIZE> buf{};
  is.read(reinterpret_cast<char*>(buf.data()), buf.size());
  if (is.gcount() != MIS_FILE_SIZE ||
      is.peek() != std::ifstream::traits_type::eof()) {
    Logger::warn("Mission file is not exactly {} bytes: {}", MIS_FILE_SIZE,
                 path.string());
    return std::nullopt;
  }

  MissionTarget target;
  target.source_name = path.filename().string();

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
    int year = milestone.date.year() + delta;
    int month = static_cast<int>(milestone.date.month());
    int day = static_cast<int>(milestone.date.day());
    // A leap day can land in a non-leap year after shifting
    if (month == 2 && day == 29 && !std::chrono::year{year}.is_leap()) {
      day = 28;
    }
    milestone.date = GameDate{year, month, day};
  }
}

bool write_mission_file(const fs::path& path,
                        const std::vector<TargetMilestone>& cv_entries) {
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
  return os.good();
}

void set_missions_directory(std::string dir) {
  missions_directory_ = std::move(dir);
}

const std::string& get_missions_directory() { return missions_directory_; }

std::vector<MissionListEntry> list_available_missions() {
  std::vector<MissionListEntry> missions;

  // Built-in missions from the original game data (m/0.mis, m/1.mis, ...).
  if (auto m_dir = fileutils::find_file_path("m")) {
    append_missions_from_dir(*m_dir, "m/", missions);
  }

  // Player-saved missions.
  append_missions_from_dir(get_missions_directory(), "", missions);

  return missions;
}

std::vector<fileutils::FileEntry> list_player_mission_files() {
  std::vector<MissionListEntry> missions;
  append_missions_from_dir(get_missions_directory(), "", missions);

  std::vector<fileutils::FileEntry> files;
  files.reserve(missions.size());
  for (const auto& mission : missions) {
    std::error_code ec;
    auto mtime = fs::last_write_time(mission.path, ec);
    files.push_back({mission.display_name, mtime});
  }
  return files;
}

std::string sanitize_mission_name(const std::string& name) {
  std::string out;
  for (char c : name) {
    const bool safe = std::isalnum(static_cast<unsigned char>(c)) || c == ' ' ||
                      c == '-' || c == '_';
    out.push_back(safe ? c : '_');
  }
  // Trim surrounding whitespace so the filename is clean
  auto first = out.find_first_not_of(' ');
  if (first == std::string::npos) {
    return {};
  }
  return out.substr(first, out.find_last_not_of(' ') - first + 1);
}

std::optional<fs::path>
save_career_mission(const std::string& name,
                    const std::vector<TargetMilestone>& cv_entries) {
  const fs::path dir = get_missions_directory();
  std::error_code ec;
  fs::create_directories(dir, ec);

  fs::path path = dir / (name + ".mis");
  if (!write_mission_file(path, cv_entries)) {
    return std::nullopt;
  }
  return path;
}

} // namespace core::mission
