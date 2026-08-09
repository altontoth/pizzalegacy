/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "core/events/LeventId.h"
#include "core/time/GameDate.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace core::events {

/**
 * Manages one-time fullscreen status messages.
 * Messages are fire-once: once scheduled for a player, the same LeventId
 * will not be scheduled again during this game. This handles both fullscreen
 * status updates, the CV entries and the awarding score points.
 */
class FullscreenMessageManager {
public:
  FullscreenMessageManager();
  ~FullscreenMessageManager() = default;

  // Schedule a one-time message for deferred delivery. Returns false if
  // already scheduled (idempotent).
  bool schedule_message(LeventId msg_id, core::PlayerId player_id,
                        const GameDate& date);

  bool was_shown(LeventId msg_id, core::PlayerId player_id) const;

  // Claim a message and scheduling it if needed. Returns false once the player
  // has already seen it.
  bool claim_for_delivery(LeventId msg_id, core::PlayerId player_id,
                          const GameDate& date);

  const EventData& get_data(LeventId msg_id) const;

  std::vector<LeventId> get_pending_for_player(core::PlayerId player_id) const;

  // Levent text for fullscreen status updates:
  std::string get_levent_text(LeventId msg_id) const;

  // Leben text for CV entries:
  std::string get_cv_text(LeventId msg_id) const;

  struct CvEntry {
    GameDate date;
    LeventId message_id;
  };

  // For save/load:
  void clear_player_messages(core::PlayerId player_id);
  void restore_scheduled(core::PlayerId pid,
                         const std::vector<CvEntry>& entries);

  // For the CV display:
  std::vector<CvEntry> get_cv_entries_for(core::PlayerId player_id) const;

private:
  bool was_scheduled(LeventId msg_id, core::PlayerId player_id) const;
  void mark_as_shown(LeventId msg_id, core::PlayerId player_id);

  struct ScheduledMessage {
    GameDate date;
    LeventId message_id;
    bool shown = false;

    ScheduledMessage(const GameDate& d, LeventId id)
        : date(d), message_id(id) {}
  };

  std::unordered_map<core::PlayerId, std::vector<ScheduledMessage>,
                     core::PlayerId::Hasher>
      player_messages_;

  mutable std::vector<std::string> levent_texts_;
  mutable bool levent_loaded_;
  mutable std::vector<std::string> leben_texts_;
  mutable bool leben_loaded_;

  void load_levent_texts() const;
  void load_leben_texts() const;
};

} // namespace core::events
