/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/restaurant/GuestWalkScene.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include "core/Demographics.h"
#include "core/restaurant/PlacedFurniture.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/util/GameContext.h"
#include "core/util/LegacyConstants.h"
#include "engine/FormatCompat.h"
#include "engine/Random.h"
#include "engine/Size.h"
#include "engine/geom/Spaces.h"
#include "gfx/ColorIndex.h"
#include "gfx/Draw.h"
#include "resources/Bob.h"
#include "resources/BobManager.h"
#include "views/base/BobRenderer.h"
#include "views/screens/restaurant/CharacterController.h"
#include "views/screens/restaurant/GuestPicker.h"
#include "views/screens/restaurant/RestaurantView.h"
#include "views/screens/restaurant/Walk.h"
#include "views/screens/restaurant/WalkNavigation.h"

using namespace LegacyConstants;

namespace views::screens::restaurant {

// Walker advances one cell every WALKER_MOVE_INTERVAL render frames
static constexpr int WALKER_MOVE_INTERVAL = 4;

void GuestWalkScene::tick() {
  if (walking_guest_) {
    walking_tick();
    return;
  }
  if (!view_.restaurant_->is_open(state_.time().time_of_day()) ||
      view_.kitchen_view_) {
    return;
  }
  // Only roll once per animation frame (every WALKER_MOVE_INTERVAL renders)
  // so the effective spawn rate matches the original
  ++spawn_anim_gate_;
  if (spawn_anim_gate_ < WALKER_MOVE_INTERVAL) {
    return;
  }
  spawn_anim_gate_ = 0;
  // Two independent 10% rolls each tick for a 1% spawn chance per tick.
  const bool roll1 = engine::random::rand_percentage_chance(10);
  const bool roll2 = engine::random::rand_percentage_chance(10);
  if (roll1 && roll2) {
    const int mode = engine::random::rand_max_n(3) + 1;

    if (mode == 1) {
      spawn_guest();
    }
    else if (mode == 2) {
      spawn_guest_departure();
    }
    else {
      spawn_waiter();
    }
  }
}

void GuestWalkScene::render() {
#ifndef NDEBUG
  if (show_walk_debug_ && dbg_walk_mode_ == 3 && !view_.kitchen_view_) {
    render_debug_full_grid();
  }
#endif
  render_walking_guest();
}

void GuestWalkScene::walking_tick() {
  auto& walker = *walking_guest_;

  // Walkers move one whole cell every WALKER_MOVE_INTERVAL render frames.
  ++walker.anim_tick;
  if (walker.anim_tick < WALKER_MOVE_INTERVAL) {
    return;
  }
  walker.anim_tick = 0;

  if (walker.path_index >= static_cast<int>(walker.path.size())) {
    switch (walker.kind) {
    case WalkingGuest::WalkerKind::GUEST:
      if (walker.walking_out) {
        const core::FurnitureId failed_id = walker.destination_chair_id;
        auto it = std::ranges::find_if(
            view_.shuffled_chairs_,
            [&](const PlacedFurniture* pf) { return pf->id() == failed_id; });
        if (it != view_.shuffled_chairs_.end()) {
          std::rotate(it, it + 1, view_.shuffled_chairs_.end());
        }
        walking_guest_.reset();
      }
      else {
        seat_arrived_guest();
      }
      break;
    case WalkingGuest::WalkerKind::GUEST_DEPARTING:
      departing_chair_id_.reset();
      walking_guest_.reset();
      break;
    case WalkingGuest::WalkerKind::WAITER_DELIVERING:
      waiter_delivered();
      break;
    case WalkingGuest::WalkerKind::WAITER_RETURNING:
    case WalkingGuest::WalkerKind::WAITER_RETURNING_WITH_PIZZA:
      walking_guest_.reset();
      break;
    }
    view_.mark_dirty();
    return;
  }

  walker.direction = walker.path[walker.path_index++];
  step_one_cell(walker.position, walker.walk_cycle, walker.direction);
  view_.mark_dirty();
}

void GuestWalkScene::seat_arrived_guest() {
  const auto& walker = *walking_guest_;
  CharacterController ctrl;
  ctrl.set_customer_type(walker.demographic);
  ctrl.set_eating(false);
  ctrl.set_guest_type(walker.guest_type);
  ctrl.set_face_index(walker.face_index);
  view_.guest_controllers_[walker.destination_chair_id] = ctrl;
  ++guest_occupancy_delta_;
  walking_guest_.reset();
  view_.mark_dirty();
}

void GuestWalkScene::spawn_guest_departure() {
  if (view_.guest_controllers_.empty()) {
    return;
  }

  auto it = view_.guest_controllers_.begin();
  const core::FurnitureId chair_id = it->first;
  const auto& ctrl = it->second;

  const PlacedFurniture* chair =
      view_.restaurant_->layout()->find_by_id(chair_id, false);
  if (!chair) {
    view_.guest_controllers_.erase(it);
    return;
  }

  const Size chair_sz = view_.bob_renderer_for(*chair).size(*chair);
  const WorldPoint from_pos{chair->origin().x + (chair_sz.w / 2),
                            chair->origin().y + (chair_sz.h / 2)};

  constexpr int tile_w = RESTAURANT_TILE_WIDTH;
  constexpr int tile_h = RESTAURANT_TILE_HEIGHT;
  const auto room_size = view_.restaurant_->size();
  const int entrance_offset = view_.restaurant_->layout()->entrance_offset();
  const int logical_x = (entrance_offset * tile_w) + (3 * tile_w / 2);
  const int entry_y = (room_size.h - 1) * tile_h;
  const int entry_cells =
      (view_.background_height_ - entry_y + WALK_CELL_SIZE - 1) /
      WALK_CELL_SIZE;

  const ConsumerType demographic = ctrl.pose().customer_type;
  const int guest_type = ctrl.guest_type();
  const int face_index = ctrl.face_index();

  const auto& walk_bob =
      context_.bob->get(compat::format("bob/walk{}.bob", guest_type));
  const int blit_offset_x = (walk_bob.width(0) / 2) + 10;
  const int blit_offset_y = (walk_bob.height(0) / 2) + 16;

  const WorldPoint exit_pos{logical_x, entry_y};
  auto wp = compute_walking_path(view_.navigation_grid(), from_pos, exit_pos,
                                 chair->origin(), chair_sz);

  std::vector<Orientation> path = std::move(wp.steps);
  path.insert(path.end(), entry_cells, Orientation::S);

  // Remove from seated state before the walker starts moving (matches
  // original).
  --guest_occupancy_delta_;
  view_.guest_controllers_.erase(it);

  // Rotate the departing chair to the back so it falls outside the occupied
  // range (0..effective_guests-1) immediately.
  {
    auto sit = std::ranges::find_if(
        view_.shuffled_chairs_,
        [&](const PlacedFurniture* pf) { return pf->id() == chair_id; });
    if (sit != view_.shuffled_chairs_.end()) {
      std::rotate(sit, sit + 1, view_.shuffled_chairs_.end());
    }
  }

  departing_chair_id_ = chair_id;

  record_walk_debug_destination(exit_pos);
  walking_guest_ = WalkingGuest{
      .position = from_pos,
      .kind = WalkingGuest::WalkerKind::GUEST_DEPARTING,
      .demographic = demographic,
      .destination = exit_pos,
      .destination_chair_id = chair_id,
      .guest_type = guest_type,
      .face_index = face_index,
      .path = std::move(path),
      .sprite_blit_offset_x = blit_offset_x,
      .sprite_blit_offset_y = blit_offset_y,
  };
  snapshot_walk_debug_grid(*walking_guest_, wp);

  view_.mark_dirty();
}

void GuestWalkScene::spawn_guest() {
  const PlacedFurniture* chair = get_guest_destination_chair();
  if (!chair) {
    return;
  }

  constexpr int tile_w = RESTAURANT_TILE_WIDTH;
  constexpr int tile_h = RESTAURANT_TILE_HEIGHT;
  const auto room_size = view_.restaurant_->size();
  const int entrance_offset = view_.restaurant_->layout()->entrance_offset();
  const int logical_x = (entrance_offset * tile_w) + (3 * tile_w / 2);
  const int entry_y = (room_size.h - 1) * tile_h;
  const int entry_cells =
      (view_.background_height_ - entry_y + WALK_CELL_SIZE - 1) /
      WALK_CELL_SIZE;
  const int spawn_y = entry_y + (entry_cells * WALK_CELL_SIZE);

  const ConsumerType demographic =
      pick_weighted_demographic(view_.restaurant_->economy().customer_counts());
  const int guest_type = pick_guest_type(demographic);

  // World pos = sprite_center + (5, 8) (from original, then 2x)
  const auto& walk_bob =
      context_.bob->get(compat::format("bob/walk{}.bob", guest_type));
  const int blit_offset_x = (walk_bob.width(0) / 2) + 10;
  const int blit_offset_y = (walk_bob.height(0) / 2) + 16;

  const Size chair_sz = view_.bob_renderer_for(*chair).size(*chair);
  const WorldPoint destination{chair->origin().x + (chair_sz.w / 2),
                               chair->origin().y + (chair_sz.h / 2)};
  const core::FurnitureId chair_id = chair->id();
  const int face_index = pick_face_index(guest_type);

  std::vector<Orientation> path(entry_cells, Orientation::N);

  record_walk_debug_destination(destination);
  auto wp = compute_walking_path(view_.navigation_grid(),
                                 WorldPoint{logical_x, entry_y}, destination,
                                 chair->origin(), chair_sz);

  bool walking_out = false;
  if (wp.steps.empty()) {
    // No path to chair: append exit steps (south) so the guest walks back out.
    path.insert(path.end(), entry_cells, Orientation::S);
    walking_out = true;
  }
  else {
    path.insert(path.end(), wp.steps.begin(), wp.steps.end());
  }

  walking_guest_ = WalkingGuest{
      .position = {logical_x, spawn_y},
      .demographic = demographic,
      .destination = destination,
      .destination_chair_id = chair_id,
      .guest_type = guest_type,
      .face_index = face_index,
      .path = std::move(path),
      .sprite_blit_offset_x = blit_offset_x,
      .sprite_blit_offset_y = blit_offset_y,
      .walking_out = walking_out,
  };
  snapshot_walk_debug_grid(*walking_guest_, wp);
}

void GuestWalkScene::spawn_waiter() {
  const PlacedFurniture* chair = find_unserved_guest_chair();
  if (!chair) {
    return;
  }

  constexpr int tile_w = RESTAURANT_TILE_WIDTH;
  constexpr int tile_h = RESTAURANT_TILE_HEIGHT;

  // +1 skips the top wall tile; KITCHEN_ENTRANCE_ROWS/2 picks the middle row.
  const int entrance_row = view_.restaurant_->layout()->kitchen_entry_offset();
  const int kitchen_y =
      ((entrance_row + 1 + KITCHEN_ENTRANCE_ROWS / 2) * tile_h) + (tile_h / 2);

  constexpr int entry_x = tile_w;
  constexpr int entry_cells = entry_x / WALK_CELL_SIZE;

  constexpr int waiter_bob_index = 12;
  const auto& walk_bob =
      context_.bob->get(compat::format("bob/walk{}.bob", waiter_bob_index));
  // World pos = sprite_center + (5, 8) (from original); at 2x: +10, +16.
  const int blit_offset_x = (walk_bob.width(0) / 2) + 10;
  const int blit_offset_y = (walk_bob.height(0) / 2) + 16;

  const Size chair_sz = view_.bob_renderer_for(*chair).size(*chair);
  const WorldPoint destination{chair->origin().x + (chair_sz.w / 2),
                               chair->origin().y + (chair_sz.h / 2)};
  const core::FurnitureId chair_id = chair->id();

  std::vector<Orientation> path(entry_cells, Orientation::E);

  record_walk_debug_destination(destination);
  auto wp = compute_walking_path(view_.navigation_grid(),
                                 WorldPoint{entry_x, kitchen_y}, destination,
                                 chair->origin(), chair_sz);
  if (wp.steps.empty()) {
    // No route: walk out a few steps then immediately return.
    constexpr int turn_steps = 3;
    std::vector<Orientation> fail_path(entry_cells + turn_steps,
                                       Orientation::E);
    fail_path.insert(fail_path.end(), entry_cells + turn_steps, Orientation::W);

    walking_guest_ = WalkingGuest{
        .position = {0, kitchen_y},
        .kind = WalkingGuest::WalkerKind::WAITER_RETURNING_WITH_PIZZA,
        .guest_type = waiter_bob_index,
        .path = std::move(fail_path),
        .sprite_blit_offset_x = blit_offset_x,
        .sprite_blit_offset_y = blit_offset_y,
    };
    return;
  }
  path.insert(path.end(), wp.steps.begin(), wp.steps.end());

  walking_guest_ = WalkingGuest{
      .position = {0, kitchen_y},
      .kind = WalkingGuest::WalkerKind::WAITER_DELIVERING,
      .destination = destination,
      .destination_chair_id = chair_id,
      .guest_type = waiter_bob_index,
      .path = std::move(path),
      .sprite_blit_offset_x = blit_offset_x,
      .sprite_blit_offset_y = blit_offset_y,
  };
  snapshot_walk_debug_grid(*walking_guest_, wp);
}

void GuestWalkScene::waiter_delivered() {
  auto& walker = *walking_guest_;

  auto it = view_.guest_controllers_.find(walker.destination_chair_id);
  if (it != view_.guest_controllers_.end()) {
    it->second.set_eating(true);
  }
  view_.mark_dirty();

  constexpr int tile_w = RESTAURANT_TILE_WIDTH;
  constexpr int tile_h = RESTAURANT_TILE_HEIGHT;
  const int entrance_row = view_.restaurant_->layout()->kitchen_entry_offset();
  const int kitchen_y =
      ((entrance_row + 1 + KITCHEN_ENTRANCE_ROWS / 2) * tile_h) + (tile_h / 2);
  constexpr int entry_x = tile_w;
  constexpr int exit_cells = entry_x / WALK_CELL_SIZE;

  // Unblock the served chair so the return path can start from inside it.
  const auto* chair = view_.restaurant_->layout()->find_by_id(
      walker.destination_chair_id, false);
  WorldPoint unblock_origin = walker.destination;
  Size unblock_size{WALK_CELL_SIZE, WALK_CELL_SIZE};
  if (chair) {
    unblock_origin = chair->origin();
    unblock_size = view_.bob_renderer_for(*chair).size(*chair);
  }

  auto wp = compute_walking_path(view_.navigation_grid(), walker.destination,
                                 WorldPoint{entry_x, kitchen_y}, unblock_origin,
                                 unblock_size);

  std::vector<Orientation> path = std::move(wp.steps);
  for (int i = 0; i < exit_cells; ++i) {
    path.push_back(Orientation::W);
  }

  walker.kind = WalkingGuest::WalkerKind::WAITER_RETURNING;
  walker.path = std::move(path);
  walker.path_index = 0;
  walker.anim_tick = 0;
  snapshot_walk_debug_grid(walker, wp);
}

const PlacedFurniture* GuestWalkScene::get_guest_destination_chair() {
  // Occupied seats occupy the first guests_to_show + guest_occupancy_delta_
  // slots.
  if (view_.shuffled_chairs_.empty()) {
    return nullptr;
  }
  const int first_empty =
      view_.restaurant_->guest_occupancy().guests + guest_occupancy_delta_;
  if (first_empty < 0 ||
      first_empty >= static_cast<int>(view_.shuffled_chairs_.size())) {
    return nullptr;
  }
  return view_.shuffled_chairs_[first_empty];
}

const PlacedFurniture* GuestWalkScene::find_unserved_guest_chair() {
  for (const auto& [chair_id, ctrl] : view_.guest_controllers_) {
    if (!ctrl.pose().eating) {
      const auto* pf = view_.restaurant_->layout()->find_by_id(chair_id, false);
      if (pf) {
        return pf;
      }
    }
  }
  return nullptr;
}

void GuestWalkScene::record_walk_debug_destination(WorldPoint destination) {
#ifndef NDEBUG
  dbg_walk_destination_ = destination;
#else
  (void)destination;
#endif
}

void GuestWalkScene::snapshot_walk_debug_grid(WalkingGuest& walker,
                                              WalkingPath& wp) {
#ifndef NDEBUG
  walker.dbg_blocked = std::move(wp.blocked);
  walker.dbg_grid_w = wp.grid_w;
  walker.dbg_grid_h = wp.grid_h;
#else
  (void)walker;
  (void)wp;
#endif
}

#ifndef NDEBUG
void GuestWalkScene::render_debug_full_grid() {
  static constexpr gfx::ColorIndex FREE_COLOR = 80;
  static constexpr gfx::ColorIndex BLOCKED_COLOR = 68;
  static constexpr int C = WALK_CELL_SIZE;

  const auto& [blocked, grid_w, grid_h] = view_.navigation_grid();

  for (int gy = 0; gy < grid_h; ++gy) {
    for (int gx = 0; gx < grid_w; ++gx) {
      const gfx::ColorIndex col =
          blocked[(gy * grid_w) + gx] ? BLOCKED_COLOR : FREE_COLOR;
      gfx::draw::box(view_.background_, ViewPoint{gx * C, gy * C},
                     ViewPoint{(gx * C) + C - 1, (gy * C) + C - 1}, col, 1);
    }
  }
}
#endif

void GuestWalkScene::render_walking_guest() {
  if (!walking_guest_ || view_.kitchen_view_) {
    return;
  }
  const auto& walker = walking_guest_.value();

  if (!view_.camera_.visible_world_rect().contains(walker.position)) {
    return;
  }

#ifndef NDEBUG
  if (show_walk_debug_) {
    static constexpr gfx::ColorIndex BLOCKED_COLOR = 71;
    static constexpr gfx::ColorIndex ANCHOR_COLOR = 15;
    static constexpr gfx::ColorIndex DEST_COLOR = 9;
    static constexpr gfx::ColorIndex AHEAD_COLOR = 104;
    static constexpr gfx::ColorIndex PAST_COLOR = 72;
    static constexpr int C = WALK_CELL_SIZE;
    static constexpr int H = C / 2;

    // mode 3 = full nav grid (drawn before this) + path overlay on top
    const bool show_grid = (dbg_walk_mode_ == 0 || dbg_walk_mode_ == 1);
    const bool show_path = (dbg_walk_mode_ != 1);
    constexpr int line_width = 1;

    // Blocked cells
    if (show_grid) {
      for (int gy = 0; gy < walker.dbg_grid_h; ++gy) {
        for (int gx = 0; gx < walker.dbg_grid_w; ++gx) {
          if (walker.dbg_blocked[(gy * walker.dbg_grid_w) + gx]) {
            gfx::draw::box(view_.background_, ViewPoint{gx * C, gy * C},
                           ViewPoint{(gx * C) + C - 1, (gy * C) + C - 1},
                           BLOCKED_COLOR, line_width);
          }
        }
      }
    }

    if (show_path && !walker.path.empty()) {
      // Get starting cell center by rewinding path_index steps.
      int px = ((walker.position.x / C) * C) + H;
      int py = ((walker.position.y / C) * C) + H;
      for (int i = walker.path_index - 1; i >= 0; --i) {
        const Orientation d = walker.path[i];
        px -= walk_dx(d) * C;
        py -= walk_dy(d) * C;
      }

      for (int i = 0; i < static_cast<int>(walker.path.size()); ++i) {
        const gfx::ColorIndex col =
            (i >= walker.path_index) ? AHEAD_COLOR : PAST_COLOR;
        const Orientation d = walker.path[i];
        const int nx = px + (walk_dx(d) * C);
        const int ny = py + (walk_dy(d) * C);

        gfx::draw::box(view_.background_, ViewPoint{px - H, py - H},
                       ViewPoint{px + H - 1, py + H - 1}, col, line_width);

        // Direction arrow:
        const int perp_x = -walk_dy(d);
        const int perp_y = walk_dx(d);
        const ViewPoint tip{px + (walk_dx(d) * 5), py + (walk_dy(d) * 5)};
        const int bx = px - (walk_dx(d) * 2);
        const int by = py - (walk_dy(d) * 2);
        gfx::draw::line(view_.background_,
                        ViewPoint{bx + (perp_x * 3), by + (perp_y * 3)}, tip,
                        col, line_width);
        gfx::draw::line(view_.background_,
                        ViewPoint{bx - (perp_x * 3), by - (perp_y * 3)}, tip,
                        col, line_width);

        gfx::draw::line(view_.background_, ViewPoint{px, py}, ViewPoint{nx, ny},
                        col, line_width);

        px = nx;
        py = ny;
      }

      // Final cell outline (path end)
      gfx::draw::box(view_.background_, ViewPoint{px - H, py - H},
                     ViewPoint{px + H - 1, py + H - 1}, AHEAD_COLOR,
                     line_width);

      // Walker anchor crosshair
      const int ax = walker.position.x;
      const int ay = walker.position.y;
      gfx::draw::line(view_.background_, ViewPoint{ax - 5, ay},
                      ViewPoint{ax + 5, ay}, ANCHOR_COLOR, line_width);
      gfx::draw::line(view_.background_, ViewPoint{ax, ay - 5},
                      ViewPoint{ax, ay + 5}, ANCHOR_COLOR, line_width);
    }

    // Draw an X for the destination
    if (dbg_walk_destination_) {
      const int dx = dbg_walk_destination_->x;
      const int dy = dbg_walk_destination_->y;
      gfx::draw::line(view_.background_, ViewPoint{dx - 5, dy - 5},
                      ViewPoint{dx + 5, dy + 5}, DEST_COLOR, line_width);
      gfx::draw::line(view_.background_, ViewPoint{dx + 5, dy - 5},
                      ViewPoint{dx - 5, dy + 5}, DEST_COLOR, line_width);
    }
  }
#endif
  const auto& guest_bob =
      context_.bob->get(compat::format("bob/walk{}.bob", walker.guest_type));
  const ViewPoint blit_pos{walker.position.x - walker.sprite_blit_offset_x,
                           walker.position.y - walker.sprite_blit_offset_y};
  view_.background_.blit_masked(
      guest_bob.frame(walk_frame_for(walker.direction, walker.walk_cycle)),
      blit_pos);

  if (walker.kind == WalkingGuest::WalkerKind::WAITER_DELIVERING ||
      walker.kind == WalkingGuest::WalkerKind::WAITER_RETURNING_WITH_PIZZA) {
    // In order of Orientation
    static constexpr std::array<std::array<int, 3>, ORIENTATION_COUNT>
        pizza_dx = {{
            {2, 1, 2},
            {5, 6, 1},
            {18, 17, 15},
            {15, 24, 18},
        }};
    static constexpr std::array<std::array<int, 3>, ORIENTATION_COUNT>
        pizza_dy = {{
            {6, 9, 3},
            {0, 0, 0},
            {-6, -7, -13},
            {7, 8, 9},
        }};

    const int dir = static_cast<int>(walker.direction);
    const int frm = WALK_CYCLE_TO_FRAME[walker.walk_cycle];
    const int x_off = pizza_dx[dir][frm];
    const int y_off = pizza_dy[dir][frm];

    const Surface pizza = view_.pizza_bob_.frame(340);
    view_.background_.blit_masked(pizza, ViewPoint{
                                             blit_pos.x + (x_off * 2),
                                             blit_pos.y + ((y_off + 8) * 2),
                                         });
  }
}

} // namespace views::screens::restaurant
