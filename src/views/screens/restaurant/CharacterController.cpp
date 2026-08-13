/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/restaurant/CharacterController.h"

#include "engine/Random.h"

using namespace views::screens::restaurant;

int CharacterController::rnd(int lo, int hi) {
  return engine::random::presentation::rand_between(lo, hi);
}

void CharacterController::reset_for_mode(bool eat) {
  pose_.eating = eat;
  if (eat) {
    phase_ = Phase::EatHold1;
    pose_.arm_state = 1;
    hold_ticks_ = rnd(EAT_BASE_MIN, EAT_BASE_MAX);
  }
  else {
    phase_ = Phase::IdleHold0;
    pose_.arm_state = 0;
    hold_ticks_ = rnd(IDLE_BASE_MIN, IDLE_BASE_MAX);
    idle_anim_steps_left_ = 0;
    idle_next_two_ = true;
  }
}

void CharacterController::tick() {
  if (hold_ticks_ > 0) {
    --hold_ticks_;
    return;
  }
  if (pose_.eating) {
    advance_eating();
  }
  else {
    advance_idle();
  }
}

void CharacterController::advance_eating() {
  switch (phase_) {
  case Phase::EatHold1:
    pose_.arm_state = 2;
    phase_ = Phase::Eat2;
    hold_ticks_ = rnd(ANIM_STEP_MIN, ANIM_STEP_MAX);
    break;
  case Phase::Eat2:
    pose_.arm_state = 3;
    phase_ = Phase::Eat3;
    hold_ticks_ = rnd(ANIM_STEP_MIN, ANIM_STEP_MAX);
    break;
  case Phase::Eat3:
    pose_.arm_state = 2;
    phase_ = Phase::Eat2Back;
    hold_ticks_ = rnd(ANIM_STEP_MIN, ANIM_STEP_MAX);
    break;
  case Phase::Eat2Back:
    pose_.arm_state = 1;
    phase_ = Phase::EatHold1;
    hold_ticks_ = rnd(EAT_BASE_MIN, EAT_BASE_MAX);
    break;
  default:
    reset_for_mode(true);
    break;
  }
}

void CharacterController::advance_idle() {
  switch (phase_) {
  case Phase::IdleHold0:
    pose_.arm_state = 1;
    phase_ = Phase::IdleHold1;
    hold_ticks_ = rnd(IDLE_PRE1_MIN, IDLE_PRE1_MAX);
    break;
  case Phase::IdleHold1:
    idle_anim_steps_left_ = 6;
    idle_next_two_ = true;
    phase_ = Phase::IdleAnim;
    [[fallthrough]];
  case Phase::IdleAnim:
    if (idle_anim_steps_left_ > 0) {
      pose_.arm_state = idle_next_two_ ? 2 : 1;
      idle_next_two_ = !idle_next_two_;
      --idle_anim_steps_left_;
      hold_ticks_ = rnd(ANIM_STEP_MIN, ANIM_STEP_MAX);
    }
    else {
      pose_.arm_state = 0;
      phase_ = Phase::IdleHold0;
      hold_ticks_ = rnd(IDLE_BASE_MIN, IDLE_BASE_MAX);
    }
    break;
  default:
    reset_for_mode(false);
    break;
  }
}
