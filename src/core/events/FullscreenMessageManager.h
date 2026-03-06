#pragma once

#include "core/common/Ids.h"
#include "core/events/LeventId.h"
#include "core/time/GameDate.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace core::events {

/**
 * Manages one-time fullscreen status messages.
 * Messages are fire-once: once scheduled for a player, the same LeventId
 * will never be scheduled again (until a new game or save load). The message
 * history is preserved across save/load.
 */
class FullscreenMessageManager {
public:
  FullscreenMessageManager();
  ~FullscreenMessageManager() = default;

  // Schedule a one-time message. Returns false if already scheduled.
  bool schedule_message(LeventId msg_id, core::PlayerId player_id,
                        const GameDate& date);

  bool was_shown(LeventId msg_id, core::PlayerId player_id) const;

  const FullscreenMessageData& get_data(LeventId msg_id) const;

  // Returns not-yet-shown messages in FIFO order.
  std::vector<LeventId> get_pending_for_player(core::PlayerId player_id) const;

  void mark_as_shown(LeventId msg_id, core::PlayerId player_id);

  // Loads from LEVENT.E file
  std::string get_message_text(LeventId msg_id) const;

  // Wipe all messages for a player (used when loading a save game to
  // clear the slate before restoring from the save file).
  void clear_player_messages(core::PlayerId player_id);

  // For save/load: returns all LeventIds ever scheduled for a player.
  std::vector<LeventId> all_scheduled_for(core::PlayerId pid) const;
  void restore_scheduled(core::PlayerId pid, const std::vector<LeventId>& ids);

private:
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

  static const std::array<FullscreenMessageData, 100> message_data_;

  mutable std::vector<std::string> message_texts_;
  mutable bool texts_loaded_;

  void load_message_texts() const;
  static size_t message_id_to_index(LeventId msg_id);
};

} // namespace core::events
