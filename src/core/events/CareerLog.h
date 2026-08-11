/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "core/events/MilestoneId.h"
#include "core/time/GameDate.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class GameState;

namespace core::events {

/**
 * Every milestone a player has reached, in the order they reached them: the
 * career shown on the CV screen. Milestones are fire-once, so a player holds
 * at most one entry per MilestoneId for the whole game.
 *
 * Each milestone has a short CV label. The ones the player is told about in
 * person also have fullscreen prose and a background to show it over.
 */
class CareerLog {
public:
  CareerLog();
  ~CareerLog() = default;

  bool was_shown(MilestoneId msg_id, core::PlayerId player_id) const;

  // Claim a message and scheduling it if needed. Returns false once the player
  // has already seen it.
  bool claim_for_delivery(MilestoneId msg_id, core::PlayerId player_id,
                          const GameDate& date);

  const EventData& get_data(MilestoneId msg_id) const;

  // Whether reaching this milestone interrupts the player with a fullscreen.
  bool has_fullscreen(MilestoneId msg_id) const;

  std::vector<MilestoneId>
  get_pending_for_player(core::PlayerId player_id) const;

  // Prose for the fullscreen, empty for milestones that only reach the CV.
  std::optional<std::string> fullscreen_text(MilestoneId msg_id) const;

  // Short label for the CV screen.
  std::string cv_label(MilestoneId msg_id) const;

  struct CvEntry {
    GameDate date;
    MilestoneId message_id;
    bool shown = false;
  };

  // For save/load:
  void clear_player_messages(core::PlayerId player_id);
  void restore_scheduled(core::PlayerId pid,
                         const std::vector<CvEntry>& entries);

  // For the CV display:
  std::vector<CvEntry> get_cv_entries_for(core::PlayerId player_id) const;

private:
  // Recording a milestone goes through GameState, which awards the score.
  friend class ::GameState;

  // Adds the CV entry. Returns false if the player already has one.
  bool schedule_message(MilestoneId msg_id, core::PlayerId player_id,
                        const GameDate& date);

  bool was_scheduled(MilestoneId msg_id, core::PlayerId player_id) const;
  void mark_as_shown(MilestoneId msg_id, core::PlayerId player_id);

  struct ScheduledMessage {
    GameDate date;
    MilestoneId message_id;
    bool shown = false;

    ScheduledMessage(const GameDate& d, MilestoneId id)
        : date(d), message_id(id) {}
  };

  std::unordered_map<core::PlayerId, std::vector<ScheduledMessage>,
                     core::PlayerId::Hasher>
      player_messages_;

  mutable std::vector<std::string> fullscreen_texts_;
  mutable bool fullscreen_texts_loaded_;
  mutable std::vector<std::string> cv_labels_;
  mutable bool cv_labels_loaded_;

  void load_fullscreen_texts() const;
  void load_cv_labels() const;
};

} // namespace core::events
