/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/mission/MissionTarget.h"
#include "core/state/GameRules.h"
#include "resources/util/FileUtils.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace core::mission {

/**
 * Binary .mis mission file I/O, byte-compatible with the original game.
 *
 * A .mis file starts with 400 bytes: four back-to-back column-major
 * 100-byte blocks, in this order:
 *
 *   offset 0    100 bytes  event_ids
 *   offset 100  100 bytes  days
 *   offset 200  100 bytes  months
 *   offset 300  100 bytes  years (biased, non-zero marks slot occupied)
 *
 * The original only reads the first 400 bytes, so in order for us to save extra
 * information about the rules that were in effect when a mission was saved, we
 * just add a trailer behind the first 400 bytes.
 */

constexpr int MIS_SLOT_COUNT = 100;
constexpr int MIS_FILE_SIZE = 4 * MIS_SLOT_COUNT;

// The year byte is (calendar year - 1900), read unsigned: 1995 is stored
// as 95. Verified against original m/N.mis files. Non-zero marks the slot
// occupied, which holds since game years are all 1994 or later.
constexpr int MIS_YEAR_BIAS = 1900;

// Parse a .mis file. Returns nullopt if the file cannot be read or is too
// short.
std::optional<MissionTarget>
read_mission_file(const std::filesystem::path& path);

// Original-game oversight: the English release starts on 1-3-1995 but
// ships the German version's mission files, whose careers start in 1994,
// so their dates can never be matched exactly. Shifts all milestone dates
// forward uniformly (preserving the intervals between them) until the
// mission starts in the game start year. Applied when fix_legacy_bugs is
// on.
void shift_mission_to_game_start(MissionTarget& target);

// Write CV entries as a .mis file (at most MIS_SLOT_COUNT entries; excess
// is dropped with a warning), followed by the rules trailer. Returns false
// on failure.
bool write_mission_file(const std::filesystem::path& path,
                        const std::vector<TargetMilestone>& cv_entries,
                        const GameRules& rules, bool rules_changed);

struct MissionListEntry {
  std::string display_name;   // e.g. "m/0.mis" or "MyCareer.mis"
  std::filesystem::path path; // absolute path
};

// Missions from the original game data "m/" directory followed by
// player-saved missions from the missions directory.
std::vector<MissionListEntry> list_available_missions();

// Player-saved missions only (the writable missions directory).
std::vector<fileutils::FileEntry> list_player_mission_files();

// Filename-safe version of a player-entered mission name; empty if
// nothing safe remains.
std::string sanitize_mission_name(const std::string& name);

// Write CV entries as <name>.mis in the missions directory (created on
// demand). The name is used as-is; sanitize player input first. Returns
// the path written, or nullopt on failure.
std::optional<std::filesystem::path>
save_career_mission(const std::string& name,
                    const std::vector<TargetMilestone>& cv_entries,
                    const GameRules& rules, bool rules_changed);

} // namespace core::mission
