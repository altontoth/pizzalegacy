/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

enum class ActionState {
  TALKING,
  ANGRY,
  HAPPY,
  VERY_HAPPY,
  WAITING,
};

enum class ActorType {
  VEGETABLE_SUPPLIER = 10,
  MEAT_SUPPLIER = 11,
  FRUIT_SUPPLIER = 12,
  CHEESE_SUPPLIER = 13,
  REALTOR = 15,
  BANK = 16,
  INSURANCE = 17,
  MAYOR = 18,
  POLICE = 19,
  MAFIA = 20,
  ARMS_DEALER = 22,
  NONE,
};

constexpr uint8_t TALKING_STATE_1 = 0;
constexpr uint8_t TALKING_STATE_2 = 1;
constexpr uint8_t TALKING_STATE_3 = 2;
constexpr uint8_t TALKING_STATE_4 = 3;
constexpr uint8_t HAPPY_STATE = 3;
constexpr uint8_t ANGRY_STATE = 4;
constexpr uint8_t BLINKING_EYE_STATE = 5;

using ActorId = int;

class Actor {
public:
  // Todo: get background_id from actor_type.
  Actor(ActorType type, ActorId id);
  ActorId id() const;
  std::string name() const;
  void name(std::string name);
  ActorType type() const;
  void head_id(int);

  int head_id() const;
  int background_id() const;
  bool is_blinking() const;
  void blinking(bool);
  void toggle_blinking();
  int state() const;
  bool next_state(); // Returns whether something changed
  void state(ActionState);
  void phone_number(uint32_t);
  uint32_t phone_number() const;
  const std::string bob_filename() const;

private:
  ActorType type_;
  // How do we identify this user:
  ActorId id_{-1};
  // What is the BOB id of this actors head?
  int head_id_{-1};
  std::string name_;
  int background_id_;
  bool blink_{false};
  std::map<ActionState, std::vector<int>>
      actions_;          // Emotion animation sequences
  int current_index_{0}; // Emotion animation index for current active animation
  ActionState action_state_{ActionState::WAITING};
  uint32_t phone_number_{0};
};
