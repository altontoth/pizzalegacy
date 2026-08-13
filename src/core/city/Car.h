/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <magic_enum/magic_enum.hpp>
#include <ostream>
#include "engine/geom/Spaces.h"

class City;

enum class Dir : uint8_t { E = 0, W = 1, N = 2, S = 3 };

inline std::ostream& operator<<(std::ostream& os, Dir dir) {
  return os << magic_enum::enum_name(dir);
}

constexpr int tile_width_pixels = 32;
constexpr int pixels_per_frame = 2;
constexpr int INITIAL_STEP_TIMER = tile_width_pixels / pixels_per_frame;
constexpr int HALF_WAY_STEP_TIMER = INITIAL_STEP_TIMER / 2;

// Margin for when to wrap cars to opposite lane
constexpr int CAR_WRAP_MARGIN_PIXELS = 12 * 2;

// Keepalive zone: cars can exist this far off-screen before being removed
// This allows cars to wrap to opposite lane and drive back
constexpr int CAR_KEEPALIVE_MARGIN_TILES = 3;
constexpr int CAR_KEEPALIVE_MARGIN_PIXELS =
    CAR_KEEPALIVE_MARGIN_TILES * tile_width_pixels;

class Car {
  friend std::ostream& operator<<(std::ostream& os, const Car& car) {
    os << "Type: " << car.car_type_ << "\n"
       << "Cooldown " << car.remaining_cooldown_ << "\n";
    return os;
  }

public:
  static Car spawn(int tile_x, int tile_y, int type, const City& city);

  // Advance one frame.
  void update(const WorldRect& view, const City& city);

  int tile_x() const { return tile_x_; }
  int tile_y() const { return tile_y_; }
  Dir dir() const { return dir_; }
  WorldPoint position() const { return position_; }
  int remaining_cooldown() const { return remaining_cooldown_; }
  int type() const { return car_type_; }
  void remaining_cooldown(int frames) { remaining_cooldown_ = frames; }
  uint8_t step_timer() const { return step_timer_; }
  void calc_screen_pos();

  static uint8_t spawn_mask(uint8_t tile_type);

private:
  Car(int tile_x, int tile_y, Dir d, int type = 0);
  [[nodiscard]] static inline bool is_left_turn(Dir from, Dir to);
  [[nodiscard]] Dir decide_desired_direction(uint8_t tile_type) const;
  Dir choose_turn(bool previous_left_turn, Dir straight_dir,
                  Dir turn_dir) const;
  static Dir direction_for_tile(uint8_t tile_type);

  int tile_x_;
  int tile_y_;
  Dir dir_;
  int car_type_;

  // intra-tile progress (uint8_t on purpose for wrap around)
  uint8_t step_timer_ = INITIAL_STEP_TIMER;

  WorldPoint position_;

  int remaining_cooldown_{0};
  bool last_turn_left_{false}; // prevent double left turns
};
