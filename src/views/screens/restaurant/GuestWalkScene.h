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
    WAITER_RETURNING_WITH_PIZZA
  };
  WorldPoint position;
  Orientation direction{Orientation::N};
  WalkerKind kind{WalkerKind::GUEST};
  // Seats the arrived guest with this demographic. Unused for waiters (whose
  // guest_type is the waiter sprite set), so they keep the default.
  ConsumerType demographic{ConsumerType::Children};
  int walk_cycle{0};
  int anim_tick{0};
  WorldPoint destination;
  core::FurnitureId destination_chair_id;
  // 0-11 = guest sprite sets, 12 = waiter; selects walk{n}.bob
  int guest_type{0};
  int face_index{0};
  std::vector<Orientation> path;
  int path_index{0};
  int sprite_blit_offset_x{0};
  int sprite_blit_offset_y{0};
  // true when no chair was reachable; exit instead of seating
  bool walking_out{false};
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

  // Remove the active walker (e.g. to clear the floor for an attack).
  void clear_walker() { walking_guest_.reset(); }

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
  void seat_arrived_guest();
  void waiter_delivered();
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
