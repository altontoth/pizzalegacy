/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>

#include "core/item/Item.h"
#include "core/restaurant/FurnitureBatch.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/UIState.h"
#include "engine/audio/AudioManager.h"
#include "engine/geom/Camera.h"
#include "engine/geom/Spaces.h"
#include "views/base/BobRenderer.h"
#include "views/base/MainPanelView.h"
#include "views/helpers/camera/CameraController.h"
#include "views/helpers/camera/RmbDragScroller.h"
#include "views/helpers/camera/ScrollEdges.h"
#include "views/screens/restaurant/CharacterController.h"
#include "views/screens/restaurant/SeatedCharacterRenderer.h"
#include "views/ui/status/StatusController.h"

// Offsets within the graphics file
#define VERTICAL_WALL_TILE_OFFSET 0
#define HORIZONTAL_WALL_TILE_OFFSET 1
#define FLOOR_TILE_OFFSET 2
#define BROWN_TILE_OFFSET 3
#define WOOD_TILE_OFFSET 7

// Offsets in the map; The offset starts counting after the vertical wall
#define KITCHEN_ENTRANCE_ROW_OFFSET 2
#define KITCHEN_ENTRANCE_ROWS 3
#define KITCHEN_BAR_ROWS 3

struct GuestRenderItem {
  WorldPoint torso_anchor_map;
  views::screens::restaurant::CharacterPose pose;
  core::FurnitureId chair_id;
  bool interacted_before{false};
  int face_index{0};
};

class RestaurantView : public MainPanelView,
                       public input::InputListener,
                       public debug::AutoLabel<RestaurantView> {
public:
  RestaurantView(GameContext& context, GameState& state,
                 RestaurantViewState r_state);
  ~RestaurantView();
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  void handle_mouse_click(WorldPoint map_point);
  void toggle_kitchen_view();
  void calculate_background_dimensions();
  void set_up_buttons();
  void update_buttons_for_furniture_state();

  struct CompetitionFlow {
    core::PlayerId human_pid;
    core::PlayerId opp_pid;
    int max_bet{0};
    int bet_dollars{0};
    int speed_level{1};
  };

  void challenge_check_and_start(core::PlayerId opp_pid);
  void challenge_ask_confirm(CompetitionFlow flow);
  void challenge_ask_bet(CompetitionFlow flow);
  void challenge_ask_speed(CompetitionFlow flow);
  void challenge_start(CompetitionFlow flow);
  void handle_throw_away();
  void handle_move_elsewhere();
  void handle_sell_furniture();
  void handle_next_item();
  void handle_previous_item();
  const BobRenderer& bob_renderer_for(const PlacedFurniture& pf) const;
  const std::vector<std::unique_ptr<PlacedFurniture>>&
  current_furniture_list() const;
  void handle_furniture_click(const PlacedFurniture* pf);
  bool intersects_wall(const WorldRect& r) const;
  void register_interactions_viewport();
  void prepare_background();

  void render_scene_passes();

  void render_sabotage(const RenderContext& ctx);

  void draw_floor_tiles(ViewRect interior);

  template <class Pred> void render_furniture_where(Pred&& pred);

  void render_placement_layer(PlacementLayer layer);
  void
  render_furniture_excluding(std::initializer_list<FurnitureCategory> cats);

  void render_guest_legs();
  void render_guest_upper();
  void render_guest_pizzas();

  bool decor_conflicts_on_table(const WorldRect& candidate_map,
                                const WorldRect& table_map_rect) const;

  std::span<const GuestRenderItem> visible_guests() const;
  void init_active_batch_if_needed();
  [[nodiscard]] FurnitureBatch* current_batch();
  void set_active_batch(size_t idx);
  void advance_to_next_batch();
  void decrement_or_erase_current_and_maybe_advance(
      bool auto_advance_batches = true);
  void throw_away_one_from_current_batch();
  std::unique_ptr<PlacedFurniture>
  make_cursor_for_batch(const FurnitureBatch& b);
  [[nodiscard]] WorldRect candidate_rect(WorldPoint p,
                                         const PlacedFurniture& ghost) const;

  [[nodiscard]] bool
  intersects_any_blocking_furniture(const WorldRect& candidate,
                                    const PlacedFurniture& cand,
                                    const PlacedFurniture*& nearest_table_out,
                                    WorldRect& nearest_table_rect_out) const;
  static ViewPoint center_of(const ViewRect& r);

  static Rotation rotation_towards(const WorldRect& from, const WorldRect& to);
  void
  face_chair_towards_nearest_table_if_any(const WorldRect& candidate,
                                          const PlacedFurniture* nearest_table,
                                          const WorldRect& table_rect);
  void place_cursor_item(WorldPoint p);
  bool can_place_now(WorldPoint p, WorldRect& out_candidate,
                     const PlacedFurniture*& out_nearest_table,
                     WorldRect& out_nearest_table_rect) const;
  void prune_empty_batches();
  void check_placement_complete_and_navigate_back();

  static std::optional<ViewRect> map_rect_to_viewport(const WorldRect& r_map,
                                                      const Camera& camera);
  void render_top_wall_and_floor();
  void render_side_and_bottom_walls();

  void render_furniture_ghost();
  WorldPoint clamp_furniture_to_interior(WorldPoint mouse_world) const;

  void zoom_out_of_restaurant_view();
  void update_status_display(const PlacedFurniture* pf);
  void update_restaurant_noise();

  const Surface& k0_;
  bool kitchen_view_ = false;
  bool closed_for_placement_ = false;
  int background_width_ = 0;
  int background_height_ = 0;
  int furniture_offset_x_ = 0; // for kitchen
  int furniture_offset_y_ = 0; // for restaurants smaller than viewport
  const BobRenderer furniture_renderer_;
  const BobRenderer oven_renderer_;
  const BobRenderer pizza_renderer_; // moebel.bob for pizza sprites
  Bob& pizza_bob_; // Direct reference for accessing individual frames
  Restaurant* restaurant_;
  Surface background_;
  std::optional<core::FurnitureId> last_clicked_furniture_index_;
  std::unique_ptr<PlacedFurniture> on_cursor_;
  RestaurantViewMode view_mode_;
  Camera camera_;

  // Furniture ghost rendering state (when holding furniture)
  std::optional<WorldPoint>
      ghost_furniture_topleft_;       // Clamped position for ghost
  bool show_furniture_cursor_{false}; // Whether custom cursor is visible
  CameraController camctrl_;
  ScrollEdges scroll_edges_{camctrl_, wood_edge_rects(panel_origin_)};
  // In RestaurantView.h (private:)
  views::screens::restaurant::SeatedCharacterRenderer guest_renderer_{context_};

  // Guest list cache (populated by visible_guests())
  mutable std::vector<GuestRenderItem> guests_cache_;

  // Shuffled linked chairs, rebuilt on each open->close transition
  mutable std::vector<const PlacedFurniture*> shuffled_chairs_;

  // Returns the index into visible_guests() for the guest on this chair, or
  // nullopt if the chair is unoccupied.  Toggles selection state.
  std::optional<int> click_guest(core::FurnitureId chair_id);

  // Tracks which chairs have already been talked to this session
  std::unordered_map<core::FurnitureId, bool, core::FurnitureId::Hasher>
      guest_talked_to_;

  // Guest animation controllers (one per chair, keyed by furniture ID)
  mutable std::unordered_map<core::FurnitureId,
                             views::screens::restaurant::CharacterController,
                             core::FurnitureId::Hasher>
      guest_controllers_;

  // Helper for passes; keeps interaction ids stable across passes
  int furniture_draw_counter_{0};
  int interior_end_row_{0};
  int top_rows_consumed_{0};
  int interior_start_row_{0};
  std::vector<FurnitureBatch>& furniture_placement_;
  std::optional<size_t> active_batch_index_; // index into furniture_placement_
  std::optional<SupplierType>
      return_to_supplier_type_; // Where to navigate after placement complete
  std::optional<audio::SoundId> current_restaurant_noise_;
  StatusController status_controller_;
  input::ListenerToken listener_;
  std::optional<core::item::Item> hovered_sabotage_item_;
  RmbDragScroller rmb_drag_scroller_;
};
