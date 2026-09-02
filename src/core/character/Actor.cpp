/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/character/Actor.h"

#include <cassert>

Actor::Actor(ActorType type, core::ActorId id)
    : type_(type), id_(id), background_id_(static_cast<int>(type)) {
  assert(id.value >= 0);

  actions_ = {
      {ActionState::TALKING, {0, 1, 2, 1}}, // Talking animation sequence
      {ActionState::ANGRY, {4, 4}},         // Angry animation sequence
      {ActionState::HAPPY, {3, 3}},         // Happy animation sequence
      {ActionState::VERY_HAPPY, {3, 0, 3}}, // Very happy animation sequence
      {ActionState::WAITING, {0}} // Neutral (default) animation sequence
  };
}

core::ActorId Actor::id() const { return id_; }

std::string Actor::name() const { return name_; }

ActorType Actor::type() const { return type_; }

int Actor::head_id() const { return head_id_; }

void Actor::head_id(int head_id) { head_id_ = head_id; }

int Actor::background_id() const { return background_id_; }

bool Actor::is_blinking() const { return blink_; }

void Actor::blinking(bool blink_state) { blink_ = blink_state; }

void Actor::toggle_blinking() { blink_ = !blink_; }

int Actor::state() const {
  return actions_.at(action_state_).at(current_index_);
};

bool Actor::next_state() {
  // Advance the current index, wrapping back to 0 if it exceeds the sequence
  // length
  int next_state = (current_index_ + 1) % actions_[action_state_].size();
  if (next_state != current_index_) {
    current_index_ = next_state;
    return true;
  }
  return false;
}

void Actor::state(ActionState new_state) {
  // Update the action state and reset the index to 0
  action_state_ = new_state;
  current_index_ = 0;
}

void Actor::phone_number(uint32_t phone_number) {
  phone_number_ = phone_number;
}

uint32_t Actor::phone_number() const { return phone_number_; }

void Actor::name(std::string name) { name_ = name; }

const std::string Actor::bob_filename() const {
  char filename[32];
  snprintf(filename, 31, "bob/typ%de.bob", head_id_);
  return std::string(filename);
}
