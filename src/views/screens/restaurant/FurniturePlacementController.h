/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "core/common/Ids.h"
#include "core/restaurant/FurnitureBatch.h"
#include "core/supply/Supplier.h"
#include "engine/geom/Spaces.h"

class GameContext;
class GameState;
class PlacedFurniture;
class RestaurantView;

namespace views::screens::restaurant {

// Furniture placement mode: the batch queue of items to place, the item on
// the cursor, the ghost preview, placement validation, and the buy/sell/
// throw-away/move handlers. Friend of RestaurantView (same pattern as
// AttackScene).
class FurniturePlacementController {
public:
  FurniturePlacementController(RestaurantView& view, GameContext& context,
                               GameState& state);

  [[nodiscard]] bool holding() const { return on_cursor_ != nullptr; }

  // Ghost preview position (world top-left) while holding furniture.
  [[nodiscard]] std::optional<WorldPoint> ghost_topleft() const {
    return ghost_furniture_topleft_;
  }

  [[nodiscard]] bool buying_mode() const {
    return return_to_supplier_type_.has_value();
  }
  void set_return_destination(std::optional<SupplierType> type) {
    return_to_supplier_type_ = type;
  }

  void init_active_batch_if_needed();

  // Configure the side buttons for the current placement state
  void update_buttons();

  // Move furniture from the layout onto the cursor.
  void pick_up(core::FurnitureId id);

  // Attempt to place the cursor item with its top-left at p.
  void try_place_at(WorldPoint p);

  // Click on placed furniture while holding: handles the decor-on-table
  // special case, otherwise places at the ghost position.
  void click_with_cursor_on(const PlacedFurniture* pf,
                            const WorldRect& furn_world,
                            WorldPoint click_world);

  // Rotate the cursor item; false when nothing is held.
  bool rotate_cursor();

  // Ghost/cursor tracking for viewport hover and leave.
  void update_ghost(WorldPoint mouse_world);
  void hide_cursor();

  // Cycle through batches with remaining items (buttons and scroll wheel).
  void handle_next_item();
  void handle_previous_item();

  // Draw the frozen ghost onto the view background.
  void render_ghost();

private:
  void handle_throw_away();
  void handle_move_elsewhere();
  void handle_sell_furniture();

  void prune_empty_batches();
  [[nodiscard]] FurnitureBatch* current_batch();
  void set_active_batch(size_t idx);
  void advance_to_next_batch();
  void decrement_or_erase_current_and_maybe_advance(
      bool auto_advance_batches = true);
  void throw_away_one_from_current_batch();
  std::unique_ptr<PlacedFurniture>
  make_cursor_for_batch(const FurnitureBatch& b);
  void refresh_furniture_cursor_image();
  void check_placement_complete_and_navigate_back();

  void place_cursor_item(WorldPoint p);
  [[nodiscard]] WorldRect candidate_rect(WorldPoint p,
                                         const PlacedFurniture& ghost) const;
  bool can_place_now(WorldPoint p, WorldRect& out_candidate,
                     const PlacedFurniture*& out_nearest_table,
                     WorldRect& out_nearest_table_rect) const;
  [[nodiscard]] bool
  intersects_any_blocking_furniture(const WorldRect& candidate,
                                    const PlacedFurniture& cand,
                                    const PlacedFurniture*& nearest_table_out,
                                    WorldRect& nearest_table_rect_out) const;
  bool intersects_wall(const WorldRect& r) const;
  bool decor_conflicts_on_table(const WorldRect& candidate_map,
                                const WorldRect& table_map_rect) const;
  void
  face_chair_towards_nearest_table_if_any(const WorldRect& candidate,
                                          const PlacedFurniture* nearest_table,
                                          const WorldRect& table_rect);
  WorldPoint clamp_furniture_to_interior(WorldPoint mouse_world) const;

  RestaurantView& view_;
  GameContext& context_;
  GameState& state_;

  std::vector<FurnitureBatch>& furniture_placement_;
  // index into furniture_placement_:
  std::optional<size_t> active_batch_index_;
  std::unique_ptr<PlacedFurniture> on_cursor_;
  // Furniture ghost rendering state (when holding furniture)
  std::optional<WorldPoint> ghost_furniture_topleft_;
  bool show_furniture_cursor_{false};
  // Where to navigate after placement complete:
  std::optional<SupplierType> return_to_supplier_type_;
};

} // namespace views::screens::restaurant
