/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

#include "core/Demographics.h"

namespace views::screens::restaurant {

enum class Part : uint8_t { Torso, Head, Legs, Arm1, Arm2, Arm3, Arm4, Count };
enum class Facing : uint8_t { Up = 0, Right = 1, Down = 2, Left = 3 };

struct CharacterPose {
  Facing facing;
  ConsumerType customer_type; // demographic (0-5), used for game logic
  int guest_type{
      0}; // sprite variant (0-11): demographic + optional +6 visual variant
  int arm_state; // 0..3 -> which arm part is "active" to render
  bool eating;   // mode
};

class CharacterController {
public:
  CharacterController() { reset_for_mode(false); }

  void set_eating(bool v) {
    if (pose_.eating != v) {
      reset_for_mode(v);
    }
  }
  void set_facing(Facing f) { pose_.facing = f; }
  void set_customer_type(ConsumerType c) { pose_.customer_type = c; }
  void set_guest_type(int t) {
    pose_.guest_type = t;
    guest_type_ = t;
  }
  int guest_type() const noexcept { return guest_type_; }
  void set_face_index(int f) { face_index_ = f; }
  int face_index() const noexcept { return face_index_; }

  void tick();
  const CharacterPose& pose() const noexcept { return pose_; }

private:
  static int rnd(int lo, int hi);
  static constexpr int EAT_BASE_MIN = 20, EAT_BASE_MAX = 50;
  static constexpr int IDLE_BASE_MIN = 40, IDLE_BASE_MAX = 90;
  static constexpr int IDLE_PRE1_MIN = 10, IDLE_PRE1_MAX = 25;
  static constexpr int ANIM_STEP_MIN = 5, ANIM_STEP_MAX = 10;

  enum class Phase : uint8_t {
    EatHold1,
    Eat2,
    Eat3,
    Eat2Back,
    IdleHold0,
    IdleHold1,
    IdleAnim
  };

  CharacterPose pose_{.facing = Facing::Up,
                      .customer_type = ConsumerType::Children,
                      .guest_type = 0,
                      .arm_state = 0,
                      .eating = false};
  int guest_type_{0};
  int face_index_{0};

  Phase phase_{Phase::IdleHold0};
  int hold_ticks_{0};
  int idle_anim_steps_left_{0};
  bool idle_next_two_{true};

  void reset_for_mode(bool eat);
  void advance_eating();
  void advance_idle();
};

} // namespace views::screens::restaurant
