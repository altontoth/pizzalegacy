#include "core/events/FullscreenMessageManager.h"

#include "resources/util/FileUtils.h"

#include <algorithm>

namespace core::events {

// Static lookup table mapping message IDs to their display data
const std::array<FullscreenMessageData, 100>
    FullscreenMessageManager::message_data_ = {
        {{7, 0},  {7, 0},  {7, 0},   {7, 0},   {7, 0},   {7, 0},  {7, 0},
         {7, 0},  {7, 0},  {7, 0},   {7, 0},   {7, 0},   {0, 0},  {0, 0},
         {8, 0},  {8, 0},  {8, 0},   {8, 0},   {8, 0},   {8, 0},  {8, 0},
         {8, 0},  {8, 0},  {8, 0},   {8, 0},   {0, 0},   {0, 0},  {6, 0},
         {0, 0},  {0, 0},  {1, 0},   {0, 0},   {5, 0},   {5, 0},  {5, 100},
         {5, 0},  {5, 50}, {5, 100}, {0, 200}, {0, 500}, {0, 0},  {0, 0},
         {5, 0},  {11, 0}, {11, 0},  {6, 0},   {6, 0},   {6, 0},  {6, 0},
         {0, 0},  {10, 0}, {10, 0},  {0, 0},   {0, 0},   {0, 0},  {0, 0},
         {0, 0},  {0, 0},  {0, 0},   {0, 0},   {0, 0},   {11, 0}, {6, 0},
         {5, 0},  {5, 0},  {5, 0},   {0, 0},   {0, 0},   {8, 0},  {8, 0},
         {8, 0},  {8, 0},  {8, 0},   {0, 0},   {0, 0},   {8, 0},  {4, 0},
         {2, 0},  {3, 0},  {0, 0},   {8, 0},   {0, 0},   {9, 0},  {0, 0},
         {11, 0}, {1, 0},  {1, 0},   {1, 0},   {1, 0},   {0, 0},  {0, 0},
         {8, 0},  {5, 0},  {5, 0},   {8, 0},   {0, 0},   {0, 0},  {0, 0},
         {0, 0},  {0, 0}}};

FullscreenMessageManager::FullscreenMessageManager() : texts_loaded_(false) {}

bool FullscreenMessageManager::schedule_message(LeventId msg_id,
                                                core::PlayerId player_id,
                                                const GameDate& date) {
  if (was_shown(msg_id, player_id)) {
    return false;
  }

  auto& messages = player_messages_[player_id];
  messages.emplace_back(date, msg_id);
  return true;
}

bool FullscreenMessageManager::was_shown(LeventId msg_id,
                                         core::PlayerId player_id) const {
  auto it = player_messages_.find(player_id);
  if (it == player_messages_.end()) {
    return false;
  }

  const auto& messages = it->second;
  return std::any_of(messages.begin(), messages.end(),
                     [msg_id](const ScheduledMessage& msg) {
                       return msg.message_id == msg_id;
                     });
}

const FullscreenMessageData&
FullscreenMessageManager::get_data(LeventId msg_id) const {
  size_t index = message_id_to_index(msg_id);
  return message_data_[index];
}

std::vector<LeventId> FullscreenMessageManager::get_pending_for_player(
    core::PlayerId player_id) const {
  std::vector<LeventId> pending;

  auto it = player_messages_.find(player_id);
  if (it == player_messages_.end()) {
    return pending;
  }

  const auto& messages = it->second;
  for (const auto& msg : messages) {
    if (!msg.shown) {
      pending.push_back(msg.message_id);
    }
  }

  return pending;
}

void FullscreenMessageManager::mark_as_shown(LeventId msg_id,
                                             core::PlayerId player_id) {
  auto it = player_messages_.find(player_id);
  if (it == player_messages_.end()) {
    return;
  }

  auto& messages = it->second;
  for (auto& msg : messages) {
    if (msg.message_id == msg_id) {
      msg.shown = true;
      break;
    }
  }
}

std::string FullscreenMessageManager::get_message_text(LeventId msg_id) const {
  if (!texts_loaded_) {
    load_message_texts();
  }

  size_t index = message_id_to_index(msg_id);
  if (index < message_texts_.size()) {
    return message_texts_[index];
  }

  return "";
}

void FullscreenMessageManager::clear_player_messages(core::PlayerId player_id) {
  player_messages_.erase(player_id);
}

std::vector<LeventId>
FullscreenMessageManager::all_scheduled_for(core::PlayerId pid) const {
  std::vector<LeventId> result;
  auto it = player_messages_.find(pid);
  if (it != player_messages_.end()) {
    for (const auto& msg : it->second) {
      result.push_back(msg.message_id);
    }
  }
  return result;
}

void FullscreenMessageManager::restore_scheduled(
    core::PlayerId pid, const std::vector<LeventId>& ids) {
  auto& messages = player_messages_[pid];
  for (auto id : ids) {
    // Restore as already-shown (these fired in a previous session)
    messages.emplace_back(GameDate{1, 1, 1}, id);
    messages.back().shown = true;
  }
}

void FullscreenMessageManager::load_message_texts() const {
  auto file = fileutils::open("t/levent.e");
  std::string line;
  while (std::getline(file, line)) {
    message_texts_.push_back(line);
  }

  texts_loaded_ = true;
}

size_t FullscreenMessageManager::message_id_to_index(LeventId msg_id) {
  // TODO: The message table is not used correctly, for now just hardcode to 1
  // so we don't get errors
  return 1;
  // return static_cast<size_t>(msg_id);
}

} // namespace core::events
