/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/events/FullscreenMessageManager.h"
#include "core/events/LeventId.h"

#include <string>
#include <vector>

namespace core::mission {

// A target milestone has the same shape as a live CV entry.
using TargetMilestone = core::events::FullscreenMessageManager::CvEntry;

/**
 * The target CV of a mission: someone else's career the player tries to
 * reproduce. Loaded from a .mis file (see MissionFile.h).
 *
 * The original copies the target into all 8 players' arrays; since every
 * copy is identical, one shared instance on GameState is equivalent.
 *
 * Always holds at least one milestone; the loader rejects empty mission
 * files (see read_mission_file). Absence of a mission is a nullopt
 * std::optional<MissionTarget> at the field level.
 */
struct MissionTarget {
  std::vector<TargetMilestone> milestones; // occupied slots, file slot order
  std::string source_name;                 // filename for display

  // The mission ends when the player achieves this event (the last
  // defined target milestone).
  core::events::LeventId final_event() const {
    return milestones.back().message_id;
  }
};

} // namespace core::mission
