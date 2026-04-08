/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/Car.h"

#include <cassert>
#include <magic_enum/magic_enum.hpp>
#include "core/city/City.h"
#include "core/util/Constants.h"

Car Car::spawn(int tile_x, int tile_y, int car_type, const City& city,
               const std::function<int(int)>& rand_int) {
  if (tile_x < 0 || tile_x >= MAP_TILES_PER_ROW || tile_y < 0 ||
      tile_y >= MAP_TILES_PER_COLUMN) {
    // Return a car with invalid position that will be removed
    // TODO: Give cars a proper cleanup flag
    return Car(-1, -1, Dir::N, car_type);
  }

  auto tile_type = city.type(tile_y * 160 + tile_x);
  return Car(tile_x, tile_y, direction_for_tile(tile_type), car_type);
}

Car::Car(int tile_x, int tile_y, Dir d, int car_type)
    : tile_x_(tile_x), tile_y_(tile_y), dir_(d), car_type_(car_type) {
  calc_screen_pos();
}

void Car::update(const WorldRect& view, const City& city,
                 const std::function<int(int)>& rand_int) {

  if (remaining_cooldown_ > 0) {
    --remaining_cooldown_;
    return;
  }

  --step_timer_;

  // If we're not at 0 yet just update the screen positions and we're done
  if (step_timer_ > 0) {
    calc_screen_pos();
    return;
  }

  // Step to the next tile
  switch (dir_) {
  case Dir::E:
    tile_x_ += 1;
    break;
  case Dir::W:
    tile_x_ -= 1;
    break;
  case Dir::N:
    tile_y_ -= 1;
    break;
  case Dir::S:
    tile_y_ += 1;
    break;
  }

  // Check if the car has driven off the screen in its direction of travel.
  constexpr int margin = CAR_WRAP_MARGIN_PIXELS;
  const int pixel_x = tile_x_ * tile_width_pixels;
  const int pixel_y = tile_y_ * tile_width_pixels;
  bool off_screen = false;
  switch (dir_) {
  case Dir::E:
    off_screen = pixel_x > view.x + view.w + margin;
    break;
  case Dir::W:
    off_screen = pixel_x < view.x - margin;
    break;
  case Dir::N:
    off_screen = pixel_y < view.y - margin;
    break;
  case Dir::S:
    off_screen = pixel_y > view.y + view.h + margin;
    break;
  }

  if (off_screen) {
    // Move to the return lane and reverse direction. Tile coordinates are
    // intentionally left just off-screen for INITIAL_STEP_TIMER ticks; no
    // map read happens during that window.
    switch (dir_) {
    case Dir::E:
      tile_y_ -= 1;
      dir_ = Dir::W;
      break;
    case Dir::W:
      tile_y_ += 1;
      dir_ = Dir::E;
      break;
    case Dir::N:
      tile_x_ -= 1;
      dir_ = Dir::S;
      break;
    case Dir::S:
      tile_x_ += 1;
      dir_ = Dir::N;
      break;
    }
    car_type_ = engine::random::rand_max_n(4);
    step_timer_ = INITIAL_STEP_TIMER;
  }
  else {
    // CAR_WRAP_MARGIN_PIXELS (24px) is less than one tile (32px), so when the
    // viewport is scrolled to the map edge a car can step past the boundary
    // before off_screen fires. Guard the map read.
    if (tile_x_ >= 0 && tile_x_ < MAP_TILES_PER_ROW && tile_y_ >= 0 &&
        tile_y_ < MAP_TILES_PER_COLUMN) {
      auto type = city.type((tile_y_ * MAP_TILES_PER_ROW) + tile_x_);
      auto new_dir = decide_desired_direction(type);
      bool did_left_turn = is_left_turn(dir_, new_dir);
      if (new_dir != dir_) {
        last_turn_left_ = did_left_turn;
      }
      dir_ = new_dir;
      step_timer_ = INITIAL_STEP_TIMER;
      if (did_left_turn) {
        step_timer_ -= 5;
      }
    }
    else {
      step_timer_ = INITIAL_STEP_TIMER;
    }
  }

  calc_screen_pos();
}

void Car::calc_screen_pos() {
  using enum Dir;

  int x = tile_x_ * tile_width_pixels;
  int y = tile_y_ * tile_width_pixels;

  // Direction-specific fine adjustment
  switch (dir_) {
  case E:
    x += 2 * (17 - step_timer_);
    y += 6;
    break;

  case W:
    x += 2 * (step_timer_ - 11);
    y += 16;
    break;

  case N:
    y += 2 * (step_timer_ - 10);
    break;

  case S:
    x += 16;
    y += 2 * (18 - step_timer_);
    break;
  }

  position_ = WorldPoint{x, y};
}

bool Car::is_left_turn(Dir from, Dir to) {
  using enum Dir;
  return (from == E && to == N) || (from == N && to == W) ||
         (from == W && to == S) || (from == S && to == E);
}

Dir Car::decide_desired_direction(uint8_t tile_type) const {
  using enum Dir;
  using enum RoadType;
  switch (static_cast<RoadType>(tile_type)) {
  case ROAD_W:
  case GLITCH_ROAD_W:
  case SHADOW_ROAD_W:
  case SHADOW2_ROAD_W:
  case SHADOW_ZEBRA_ROAD_E:
  case PARTIAL_SHADOW_ROAD_W:
  case CORNER_W:
    return W;

  case ROAD_E:
  case GLITCH_ROAD_E:
  case SHADOW2_ROAD_E:
  case ZEBRA_ROAD_E:
  case CORNER_E:
    return E;

  case ROAD_N:
  case PARTIAL_SHADOW_ROAD_N:
  case SHADOW_ROAD_N:
  case SHADOW_ZEBRA_ROAD_N:
  case CORNER_N:
  case SHADOW_CORNER_N:
    return N;

  case ROAD_S:
  case SHADOW_ROAD_S:
  case ZEBRA_ROAD_S:
  case CORNER_S:
  case SHADOW_CORNER_E:
    return S;

  case CORNER_SW:
  case SHADOW_CORNER_S:
    return choose_turn(last_turn_left_, W, S);

  case CORNER_SE:
    return choose_turn(last_turn_left_, S, E);

  case CORNER_NW:
  case SHADOW_CORNER_NW:
    return choose_turn(last_turn_left_, W, N);

  case CORNER_NE:
    return choose_turn(last_turn_left_, E, N);

  default:
    return dir_;
  }
}

Dir Car::direction_for_tile(uint8_t tile_type) {
  uint8_t mask = spawn_mask(tile_type);
  assert(mask && "Mask is false"); // we promised only spawnable tiles

  std::array<Dir, 4> dirs{};
  int n = 0;
  for (int b = 0; b < 4; ++b) {
    if (mask & (1u << b)) {
      dirs[n++] = static_cast<Dir>(b);
    }
  }

  return dirs[engine::random::rand_max_n(n)];
}

Dir Car::choose_turn(bool previous_left_turn, Dir straight_dir,
                     Dir turn_dir) const {
  // If we already made a left turn on *this* tile, we MUST go straight.
  if (previous_left_turn) {
    return straight_dir;
  }

  // Otherwise toss a coin
  bool pickTurn = engine::random::rand_percentage_chance(50);

  return pickTurn ? turn_dir : straight_dir;
}

uint8_t Car::spawn_mask(uint8_t tile_type) {
  const uint8_t E = 1 << static_cast<int>(Dir::E);
  const uint8_t W = 1 << static_cast<int>(Dir::W);
  const uint8_t S = 1 << static_cast<int>(Dir::S);
  const uint8_t N = 1 << static_cast<int>(Dir::N);
  switch (static_cast<RoadType>(tile_type)) {
  // one-way roads
  case RoadType::ROAD_E:
  case RoadType::GLITCH_ROAD_E:
  case RoadType::SHADOW2_ROAD_E:
  case RoadType::SHADOW_ZEBRA_ROAD_E:
  case RoadType::CORNER_E:
  case RoadType::ZEBRA_ROAD_E:
  case RoadType::SHADOW_CORNER_E:
    return E;

  case RoadType::ROAD_W:
  case RoadType::GLITCH_ROAD_W:
  case RoadType::SHADOW2_ROAD_W:
  case RoadType::CORNER_W:
  case RoadType::PARTIAL_SHADOW_ROAD_W:
  case RoadType::SHADOW_ROAD_W:
    return W;

  case RoadType::ROAD_S:
  case RoadType::SHADOW_ROAD_S:
  case RoadType::CORNER_S:
  case RoadType::ZEBRA_ROAD_S:
  case RoadType::SHADOW_CORNER_S:
    return S;

  case RoadType::ROAD_N:
  case RoadType::SHADOW_ROAD_N:
  case RoadType::PARTIAL_SHADOW_ROAD_N:
  case RoadType::SHADOW_ZEBRA_ROAD_N:
  case RoadType::CORNER_N:
  case RoadType::SHADOW_CORNER_N:
    return N;

  // two-way corners
  case RoadType::CORNER_SW:
    return S | W;
  case RoadType::CORNER_SE:
    return S | E;
  case RoadType::CORNER_NW:
  case RoadType::SHADOW_CORNER_NW:
    return N | W;
  case RoadType::CORNER_NE:
    return N | E;

  case RoadType::NO_ROAD:
    return 0;
  }
  return 0;
}
