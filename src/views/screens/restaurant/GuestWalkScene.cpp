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

// Render frames per animation frame; a walker advances one cell each
// animation frame.
static constexpr int WALKER_MOVE_INTERVAL = 4;

// The kitchen doorway is one tile deep.
static constexpr int KITCHEN_DOOR_CELLS =
    RESTAURANT_TILE_WIDTH / WALK_CELL_SIZE;

void GuestWalkScene::tick() {
  if (walking_guest_) {
    walking_tick();
    return;
  }
  if (!view_.restaurant_->is_open(state_.time().time_of_day()) ||
      view_.kitchen_view_) {
    return;
  }
  // Only roll once per animation frame so the effective spawn rate matches the
  // original.
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

  ++walker.anim_tick;
  if (walker.anim_tick < WALKER_MOVE_INTERVAL) {
    return;
  }
  walker.anim_tick = 0;

  while (walker.path_index >= static_cast<int>(walker.path.size())) {
    if (!advance_leg(walker)) {
      return;
    }
  }

  // The original re-pathfinds at every waypoint, so look for a better route
  // each time the route turns a corner.
  if (walker.phase == WalkingGuest::Phase::NAVIGATING &&
      walker.path_index > 0 &&
      walker.path[walker.path_index] != walker.path[walker.path_index - 1]) {
    try_replan(walker);
  }

  walker.direction = walker.path[walker.path_index++];
  step_one_cell(walker.position, walker.walk_cycle, walker.direction);
  if (walker.phase == WalkingGuest::Phase::NAVIGATING) {
    walker.trail.push_back(walker.direction);
  }
  view_.mark_dirty();
}

bool GuestWalkScene::advance_leg(WalkingGuest& walker) {
  switch (walker.phase) {
  case WalkingGuest::Phase::SCRIPTED_IN:
    walker.phase = WalkingGuest::Phase::NAVIGATING;
    return plan_route(walker) ? true : give_up(walker);
  case WalkingGuest::Phase::NAVIGATING:
    // Routes always end on the goal cell, so a spent route means arrival.
    return goal_reached(walker);
  case WalkingGuest::Phase::SCRIPTED_OUT:
    return finish();
  }
  return finish();
}

bool GuestWalkScene::plan_route(WalkingGuest& walker) {
  auto wp = compute_walking_path(view_.navigation_grid(), walker.position,
                                 walker.goal, walker.goal_passable);
  snapshot_walk_debug_grid(walker, wp);
  if (!wp.reachable) {
    return false;
  }
  walker.path = std::move(wp.steps);
  walker.path_index = 0;
  return true;
}

// Adopt a fresh route only when it is shorter than what is left of the
// current one.
void GuestWalkScene::try_replan(WalkingGuest& walker) {
  const int remaining =
      static_cast<int>(walker.path.size()) - walker.path_index;
  auto wp = compute_walking_path(view_.navigation_grid(), walker.position,
                                 walker.goal, walker.goal_passable);
  if (!wp.reachable || wp.steps.empty() ||
      static_cast<int>(wp.steps.size()) >= remaining) {
    return;
  }
  snapshot_walk_debug_grid(walker, wp);
  walker.path = std::move(wp.steps);
  walker.path_index = 0;
}

bool GuestWalkScene::give_up(WalkingGuest& walker) {
  if (walker.kind == WalkingGuest::WalkerKind::WAITER_DELIVERING) {
    // Abandon the delivery and head back to the kitchen; the guest keeps
    // waiting for a pizza.
    walker.kind = WalkingGuest::WalkerKind::WAITER_RETURNING_WITH_PIZZA;
    walker.goal = kitchen_door();
    return plan_route(walker) ? true : begin_scripted_out(walker);
  }
  if (walker.kind == WalkingGuest::WalkerKind::GUEST) {
    retire_chair(walker.chair_id);
  }
  return begin_scripted_out(walker);
}

bool GuestWalkScene::goal_reached(WalkingGuest& walker) {
  switch (walker.kind) {
  case WalkingGuest::WalkerKind::GUEST:
    seat_arrived_guest();
    return finish();
  case WalkingGuest::WalkerKind::WAITER_DELIVERING:
    return waiter_delivered(walker);
  case WalkingGuest::WalkerKind::GUEST_DEPARTING:
  case WalkingGuest::WalkerKind::WAITER_RETURNING:
  case WalkingGuest::WalkerKind::WAITER_RETURNING_WITH_PIZZA:
    return begin_scripted_out(walker);
  }
  return finish();
}

bool GuestWalkScene::begin_scripted_out(WalkingGuest& walker) {
  walker.phase = WalkingGuest::Phase::SCRIPTED_OUT;
  walker.path = std::move(walker.tail);
  walker.path_index = 0;
  if (walker.path.empty()) {
    return finish();
  }
  return true;
}

bool GuestWalkScene::finish() {
  if (walking_guest_->kind == WalkingGuest::WalkerKind::GUEST_DEPARTING) {
    departing_chair_id_.reset();
  }
  walking_guest_.reset();
  view_.mark_dirty();
  return false;
}

// Move a chair to the back of the seating order so it falls outside the
// occupied range (0..effective_guests-1).
void GuestWalkScene::retire_chair(core::FurnitureId chair_id) {
  auto it = std::ranges::find_if(
      view_.shuffled_chairs_,
      [&](const PlacedFurniture* pf) { return pf->id() == chair_id; });
  if (it != view_.shuffled_chairs_.end()) {
    std::rotate(it, it + 1, view_.shuffled_chairs_.end());
  }
}

void GuestWalkScene::seat_arrived_guest() {
  const auto& walker = *walking_guest_;
  CharacterController ctrl;
  ctrl.set_customer_type(walker.demographic);
  ctrl.set_eating(false);
  ctrl.set_guest_type(walker.guest_type);
  ctrl.set_face_index(walker.face_index);
  view_.guest_controllers_[walker.chair_id] = ctrl;
  ++guest_occupancy_delta_;
}

void GuestWalkScene::spawn_guest_departure() {
  const WorldPoint exit_pos = customer_door();

  auto it = view_.guest_controllers_.begin();
  if (it == view_.guest_controllers_.end()) {
    return;
  }
  const PlacedFurniture* chair =
      view_.restaurant_->layout()->find_by_id(it->first, false);
  if (!chair) {
    view_.guest_controllers_.erase(it);
    return;
  }

  const WorldRect chair_rect = furniture_rect(*chair);
  WalkingPath route = compute_walking_path(
      view_.navigation_grid(), chair_rect.center(), exit_pos, chair_rect);
  if (!route.reachable) {
    return;
  }

  const core::FurnitureId chair_id = chair->id();
  const ConsumerType demographic = it->second.pose().customer_type;
  const int guest_type = it->second.guest_type();
  const int face_index = it->second.face_index();
  const ViewPoint anchor = sprite_anchor_offset(guest_type);

  // Remove from seated state before the walker starts moving (matches
  // original).
  --guest_occupancy_delta_;
  view_.guest_controllers_.erase(it);
  retire_chair(chair_id);
  departing_chair_id_ = chair_id;

  record_walk_debug_destination(exit_pos);
  walking_guest_ = WalkingGuest{
      .position = chair_rect.center(),
      .kind = WalkingGuest::WalkerKind::GUEST_DEPARTING,
      .phase = WalkingGuest::Phase::NAVIGATING,
      .demographic = demographic,
      .goal = exit_pos,
      .goal_passable = chair_rect,
      .chair_id = chair_id,
      .guest_type = guest_type,
      .face_index = face_index,
      .path = std::move(route.steps),
      .tail = std::vector<Orientation>(customer_door_cells(anchor.y),
                                       Orientation::S),
      .sprite_blit_offset_x = anchor.x,
      .sprite_blit_offset_y = anchor.y,
  };
  snapshot_walk_debug_grid(*walking_guest_, route);

  view_.mark_dirty();
}

void GuestWalkScene::spawn_guest() {
  const PlacedFurniture* chair = get_guest_destination_chair();
  if (!chair) {
    return;
  }

  const WorldPoint door = customer_door();

  const ConsumerType demographic =
      pick_weighted_demographic(view_.restaurant_->economy().customer_counts());
  const int guest_type = pick_guest_type(demographic);
  const ViewPoint anchor = sprite_anchor_offset(guest_type);
  const int lead_cells = customer_door_cells(anchor.y);
  const WorldRect chair_rect = furniture_rect(*chair);

  record_walk_debug_destination(chair_rect.center());
  walking_guest_ = WalkingGuest{
      .position = {door.x, door.y + (lead_cells * WALK_CELL_SIZE)},
      .demographic = demographic,
      .goal = chair_rect.center(),
      .goal_passable = chair_rect,
      .chair_id = chair->id(),
      .guest_type = guest_type,
      .face_index = pick_face_index(guest_type),
      .path = std::vector<Orientation>(lead_cells, Orientation::N),
      .tail = std::vector<Orientation>(lead_cells, Orientation::S),
      .sprite_blit_offset_x = anchor.x,
      .sprite_blit_offset_y = anchor.y,
  };
}

void GuestWalkScene::spawn_waiter() {
  const PlacedFurniture* chair = find_unserved_guest_chair();
  if (!chair) {
    return;
  }

  const WorldPoint door = kitchen_door();
  constexpr int waiter_bob_index = 12;
  const ViewPoint anchor = sprite_anchor_offset(waiter_bob_index);
  const WorldRect chair_rect = furniture_rect(*chair);

  record_walk_debug_destination(chair_rect.center());
  walking_guest_ = WalkingGuest{
      .position = {door.x - (KITCHEN_DOOR_CELLS * WALK_CELL_SIZE), door.y},
      .kind = WalkingGuest::WalkerKind::WAITER_DELIVERING,
      .goal = chair_rect.center(),
      .goal_passable = chair_rect,
      .chair_id = chair->id(),
      .guest_type = waiter_bob_index,
      .path = std::vector<Orientation>(KITCHEN_DOOR_CELLS, Orientation::E),
      .tail = std::vector<Orientation>(KITCHEN_DOOR_CELLS, Orientation::W),
      .sprite_blit_offset_x = anchor.x,
      .sprite_blit_offset_y = anchor.y,
  };
}

bool GuestWalkScene::waiter_delivered(WalkingGuest& walker) {
  auto it = view_.guest_controllers_.find(walker.chair_id);
  if (it != view_.guest_controllers_.end()) {
    it->second.set_eating(true);
  }
  view_.mark_dirty();

  walker.kind = WalkingGuest::WalkerKind::WAITER_RETURNING;
  walker.goal = kitchen_door();
  std::vector<Orientation> outbound = std::move(walker.trail);
  walker.trail.clear();
  if (plan_route(walker)) {
    return true;
  }
  // The greedy search can miss a route that exists, so retrace the steps that
  // got the waiter here.
  walker.path.assign(outbound.rbegin(), outbound.rend());
  for (Orientation& step : walker.path) {
    step = opposite(step);
  }
  walker.path_index = 0;
  return walker.path.empty() ? begin_scripted_out(walker) : true;
}

WorldPoint GuestWalkScene::customer_door() const {
  const int entrance_offset = view_.restaurant_->layout()->entrance_offset();
  return {(entrance_offset * RESTAURANT_TILE_WIDTH) +
              (3 * RESTAURANT_TILE_WIDTH / 2),
          (view_.restaurant_->size().h - 1) * RESTAURANT_TILE_HEIGHT};
}

WorldPoint GuestWalkScene::kitchen_door() const {
  // +1 skips the top wall tile; KITCHEN_ENTRANCE_ROWS/2 picks the middle row.
  const int entrance_row = view_.restaurant_->layout()->kitchen_entry_offset();
  return {RESTAURANT_TILE_WIDTH,
          ((entrance_row + 1 + (KITCHEN_ENTRANCE_ROWS / 2)) *
           RESTAURANT_TILE_HEIGHT) +
              (RESTAURANT_TILE_HEIGHT / 2)};
}

int GuestWalkScene::customer_door_cells(int sprite_anchor_y) const {
  const int distance =
      view_.background_height_ - customer_door().y + sprite_anchor_y;
  return (distance + WALK_CELL_SIZE - 1) / WALK_CELL_SIZE;
}

WorldRect GuestWalkScene::furniture_rect(const PlacedFurniture& pf) const {
  return {pf.origin(), view_.bob_renderer_for(pf).size(pf)};
}

// World pos sits 10 right and 16 down from the sprite center, the original's
// offset at our scale.
ViewPoint GuestWalkScene::sprite_anchor_offset(int guest_type) const {
  const auto& walk_bob =
      context_.bob->get(compat::format("bob/walk{}.bob", guest_type));
  return {(walk_bob.width(0) / 2) + 10, (walk_bob.height(0) / 2) + 16};
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

  const auto& guest_bob =
      context_.bob->get(compat::format("bob/walk{}.bob", walker.guest_type));
  const int frame = walk_frame_for(walker.direction, walker.walk_cycle);
  const ViewPoint blit_pos{walker.position.x - walker.sprite_blit_offset_x,
                           walker.position.y - walker.sprite_blit_offset_y};

  const WorldRect sprite_rect{
      blit_pos.x, blit_pos.y, guest_bob.width(static_cast<std::size_t>(frame)),
      guest_bob.height(static_cast<std::size_t>(frame))};
  if (!view_.camera_.possibly_visible(sprite_rect)) {
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
  view_.background_.blit_masked(guest_bob.frame(frame), blit_pos);

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
