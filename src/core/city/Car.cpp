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

  // We're at a tile border, move to the next tile
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

  // Check boundaries on each axis separately
  int pixel_x = tile_x_ * tile_width_pixels;
  int pixel_y = tile_y_ * tile_width_pixels;
  constexpr int wrap_margin = CAR_WRAP_MARGIN_PIXELS;

  bool out_of_x_bounds =
      pixel_x < view.x - wrap_margin || pixel_x > view.x + view.w + wrap_margin;
  bool out_of_y_bounds =
      pixel_y < view.y - wrap_margin || pixel_y > view.y + view.h + wrap_margin;

  // Determine if we should wrap based on direction and travel axis
  bool should_wrap = false;
  if (dir_ == Dir::E || dir_ == Dir::W) {
    // East/West: only wrap when leaving X-axis bounds
    should_wrap = out_of_x_bounds;
  }
  else {
    // North/South: only wrap when leaving Y-axis bounds
    should_wrap = out_of_y_bounds;
  }

  if (should_wrap) {
    // Crossed boundary on our travel axis - attempt to wrap to opposite lane
    if (!randomize_car_and_turn_around(city)) {
      // Opposite lane is not a road - mark for removal by setting position far
      // out
      step_timer_ = INITIAL_STEP_TIMER;
      position_ = WorldPoint{-1, -1};
      return;
    }
  }
  else {
    // Car is within keepalive bounds on travel axis
    // Check if visible for tile logic
    bool is_visible = !new_tiles_are_out_of_view(view, tile_x_, tile_y_);

    // Also check if tile is within map bounds
    bool is_in_bounds = (tile_x_ >= 0 && tile_x_ < MAP_TILES_PER_ROW &&
                         tile_y_ >= 0 && tile_y_ < MAP_TILES_PER_COLUMN);

    if (is_visible && is_in_bounds) {
      // Normal in-view tile logic
      auto index = tile_y_ * 160 + tile_x_;
      auto type = city.type(index);
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
      // Car is in keepalive zone but not visible - just keep driving
      step_timer_ = INITIAL_STEP_TIMER;
    }
  }

  calc_screen_pos();
}

bool Car::randomize_car_and_turn_around(const City& city) {
  using enum Dir;

  auto old_tile_x = tile_x_;
  auto old_tile_y = tile_y_;
  auto old_dir = dir_;

  // Move to the opposite lane and reverse direction
  switch (dir_) {
  case E:
    tile_y_ -= 1; // East/West roads: switch lanes via Y
    dir_ = W;     // Reverse to West
    break;
  case W:
    tile_y_ += 1;
    dir_ = E; // Reverse to East
    break;
  case N:
    tile_x_ -= 1; // North/South roads: switch lanes via X
    dir_ = S;     // Reverse to South
    break;
  case S:
    tile_x_ += 1;
    dir_ = N; // Reverse to North
    break;
  }

  // Check if the new position is within map bounds
  if (tile_x_ < 0 || tile_x_ >= MAP_TILES_PER_ROW || tile_y_ < 0 ||
      tile_y_ >= MAP_TILES_PER_COLUMN) {
    // New position is out of bounds - restore old position and return false
    tile_x_ = old_tile_x;
    tile_y_ = old_tile_y;
    dir_ = old_dir;
    return false;
  }

  // Check if the new tile is a road
  auto new_index = (tile_y_ * 160) + tile_x_;
  auto new_tile_type = city.type(new_index);
  auto spawn_mask_value = spawn_mask(new_tile_type);

  if (spawn_mask_value == 0) {
    // Opposite lane is not a road - restore position and return false
    tile_x_ = old_tile_x;
    tile_y_ = old_tile_y;
    dir_ = old_dir;
    return false;
  }

  car_type_ = engine::random::rand_max_n(4);
  step_timer_ = INITIAL_STEP_TIMER;
  return true;
}

bool Car::new_tiles_are_out_of_view(const WorldRect& view, int tile_x,
                                    int tile_y) const {
  int pixel_x = tile_x * tile_width_pixels;
  int pixel_y = tile_y * tile_width_pixels;

  constexpr int margin = CAR_WRAP_MARGIN_PIXELS;

  return pixel_x < view.x - margin || pixel_y < view.y - margin ||
         pixel_x >= (view.x + view.w + margin) ||
         pixel_y >= (view.y + view.h + margin);
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
