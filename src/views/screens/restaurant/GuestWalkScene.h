/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <vector>

#include "core/Demographics.h"
#include "core/common/Ids.h"
#include "engine/geom/Spaces.h"
#include "views/screens/restaurant/Walk.h"
#include "views/screens/restaurant/WalkNavigation.h"

class GameContext;
class GameState;
class PlacedFurniture;
class RestaurantView;

namespace views::screens::restaurant {

struct WalkingGuest {
  enum class WalkerKind {
    GUEST,
    GUEST_DEPARTING,
    WAITER_DELIVERING,
    WAITER_RETURNING,
    WAITER_RETURNING_WITH_PIZZA,
  };
  // A trip runs in legs: fixed steps through the doorway onto the navigation
  // grid, grid navigation toward the goal, then fixed steps back off the grid.
  // A walker already on the grid starts at NAVIGATING.
  enum class Phase { SCRIPTED_IN, NAVIGATING, SCRIPTED_OUT };

  WorldPoint position;
  Orientation direction{Orientation::N};
  WalkerKind kind{WalkerKind::GUEST};
  Phase phase{Phase::SCRIPTED_IN};
  // Seats the arrived guest with this demographic. Unused for waiters (whose
  // guest_type is the waiter sprite set), so they keep the default.
  ConsumerType demographic{ConsumerType::Children};
  int walk_cycle{0};
  int anim_tick{0};
  WorldPoint goal;
  // The chair rect, passable while planning so a path can enter or leave it.
  WorldRect goal_passable;
  // The chair this trip is about: the seat to take, to leave, or to serve.
  core::FurnitureId chair_id;
  // 0-11 = guest sprite sets, 12 = waiter; selects walk{n}.bob
  int guest_type{0};
  int face_index{0};
  // Steps of the current leg, and the leg that walks back off the grid.
  std::vector<Orientation> path;
  int path_index{0};
  std::vector<Orientation> tail;
  // Steps walked on the navigation leg.
  std::vector<Orientation> trail;
  int sprite_blit_offset_x{0};
  int sprite_blit_offset_y{0};
#ifndef NDEBUG
  std::vector<bool> dbg_blocked;
  int dbg_grid_w{0};
  int dbg_grid_h{0};
#endif
};

// Ambient walkers in the dining room: guests arriving/departing and waiters
// delivering pizzas. Owns spawning, path following, the seating handoff into
// the view's guest controllers, and walker sprite rendering. Friend of
// RestaurantView (same pattern as AttackScene).
class GuestWalkScene {
public:
  GuestWalkScene(RestaurantView& view, GameContext& context, GameState& state)
      : view_(view), context_(context), state_(state) {}

  // Advance the active walker, or occasionally spawn one while open.
  void tick();
  // Draw the walker (and walk debug overlays) onto the view background.
  void render();

  // Chair whose guest is currently walking out; excluded from seating.
  std::optional<core::FurnitureId> departing_chair_id() const {
    return departing_chair_id_;
  }

  // Seated-guest count adjustment from walkers that arrived or departed on
  // top of the restaurant's occupancy figure.
  int occupancy_delta() const { return guest_occupancy_delta_; }
  void reset_occupancy_delta() { guest_occupancy_delta_ = 0; }

#ifndef NDEBUG
  void toggle_walk_debug() { show_walk_debug_ = !show_walk_debug_; }
#endif

private:
  void walking_tick();
  void spawn_guest();
  void spawn_guest_departure();
  void spawn_waiter();
  // Each of these starts the walker's next leg and returns false once the
  // walker has finished and been removed.
  bool advance_leg(WalkingGuest& walker);
  bool give_up(WalkingGuest& walker);
  bool goal_reached(WalkingGuest& walker);
  bool begin_scripted_out(WalkingGuest& walker);
  bool waiter_delivered(WalkingGuest& walker);
  bool finish();
  // False when the goal is unreachable.
  bool plan_route(WalkingGuest& walker);
  void try_replan(WalkingGuest& walker);
  void seat_arrived_guest();
  void retire_chair(core::FurnitureId chair_id);
  WorldRect furniture_rect(const PlacedFurniture& pf) const;
  // Offset from a walker's world position to the top left of its sprite.
  ViewPoint sprite_anchor_offset(int guest_type) const;
  // Where walkers join and leave the navigation grid.
  WorldPoint customer_door() const;
  WorldPoint kitchen_door() const;
  // Length of the fixed leg between the customer door and the edge of the
  // drawn room.
  int customer_door_cells() const;
  const PlacedFurniture* get_guest_destination_chair();
  const PlacedFurniture* find_unserved_guest_chair();
  void render_walking_guest();
#ifndef NDEBUG
  void render_debug_full_grid();
#endif
  void record_walk_debug_destination(WorldPoint destination);
  void snapshot_walk_debug_grid(WalkingGuest& walker, WalkingPath& wp);

  RestaurantView& view_;
  GameContext& context_;
  GameState& state_;

  std::optional<WalkingGuest> walking_guest_;
  std::optional<core::FurnitureId> departing_chair_id_;
  int spawn_anim_gate_{0};
  int guest_occupancy_delta_{0};
#ifndef NDEBUG
  bool show_walk_debug_{false};
  // 0=grid+path, 1=grid only, 2=path only, 3=full nav grid
  int dbg_walk_mode_{3};
  std::optional<WorldPoint> dbg_walk_destination_; // last attempted destination
#endif
};

} // namespace views::screens::restaurant
