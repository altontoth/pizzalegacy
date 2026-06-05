/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/Demographics.h"
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

struct WalkingGuest {
  enum class Orientation { S = 0, W = 1, N = 2, E = 3 };
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
  // true when no chair was reachable; walker exits instead of seating
  bool walking_out{false};
#ifndef NDEBUG
  std::vector<bool> dbg_blocked;
  int dbg_grid_w{0};
  int dbg_grid_h{0};
#endif
};

struct AttackSprite {
  WorldPoint position;
  WalkingGuest::Orientation direction{WalkingGuest::Orientation::N};
  int walk_cycle{0};
  int move_gate{0};
  int sprite_blit_offset_x{0};
  int sprite_blit_offset_y{0};
  bool is_defender{false};
  // TODO(attack): health, weapon type, weapon_frame and combat state
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
  void refresh_furniture_cursor_image();
  const BobRenderer& bob_renderer_for(const PlacedFurniture& pf) const;
  const std::vector<std::unique_ptr<PlacedFurniture>>&
  current_furniture_list() const;
  void handle_furniture_click(const PlacedFurniture* pf);
  bool intersects_wall(const WorldRect& r) const;
  void register_interactions_viewport();
  void prepare_background();

  void render_scene_passes();

  void render_sabotage(const RenderContext& ctx);
  void render_weapon_select(const RenderContext& ctx);
  // weapon and sabotage item picker
  void render_item_picker(const RenderContext& ctx, std::string_view title,
                          std::span<const core::item::Item> items,
                          const std::function<void(core::item::Item)>& on_pick);
  void execute_sabotage_attack(core::item::Item item);
  void render_walking_guest();
  void render_attackers();

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

  void rebuild_guest_cache();
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
  static int walk_frame_for(const WalkingGuest& g);
  static int walk_frame_for(WalkingGuest::Orientation dir, int walk_cycle);

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
#ifndef NDEBUG
  void render_debug_full_grid();
#endif

  void render_furniture_ghost();
  WorldPoint clamp_furniture_to_interior(WorldPoint mouse_world) const;

  void zoom_out_of_restaurant_view();
  void update_status_display(const PlacedFurniture* pf);
  void update_restaurant_noise();
  void spawn_guest();
  void spawn_guest_departure();
  void spawn_waiter();
  void walking_tick();
  void waiter_delivered();
  void begin_attack(core::item::Item weapon);
  void attack_tick();
  const PlacedFurniture* get_guest_destination_chair();
  const PlacedFurniture* find_unserved_guest_chair();
  struct NavGrid {
    std::vector<bool> blocked;
    int w{0};
    int h{0};
  };
  struct WalkingPath {
    std::vector<WalkingGuest::Orientation> steps;
    std::vector<bool> blocked;
    int grid_w{0};
    int grid_h{0};
  };
  [[nodiscard]] NavGrid build_navigation_grid() const;
  [[nodiscard]] WalkingPath compute_walking_path(WorldPoint from, WorldPoint to,
                                                 WorldPoint dest_origin,
                                                 Size dest_size) const;

  void record_walk_debug_destination(WorldPoint destination);
  void snapshot_walk_debug_grid(WalkingGuest& walker, WalkingPath& wp);

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
  std::optional<WorldPoint> ghost_furniture_topleft_;
  bool show_furniture_cursor_{false};
  CameraController camctrl_;
  ScrollEdges scroll_edges_{camctrl_, wood_edge_rects(panel_origin_)};
  views::screens::restaurant::SeatedCharacterRenderer guest_renderer_{context_};
  std::vector<GuestRenderItem> guests_cache_;
  std::vector<const PlacedFurniture*> shuffled_chairs_;

  // Returns the index into guests_cache_ for the guest on this chair, or
  // nullopt if the chair is unoccupied. Toggles selection state.
  std::optional<int> click_guest(core::FurnitureId chair_id);

  std::unordered_map<core::FurnitureId, bool, core::FurnitureId::Hasher>
      guest_talked_to_;

  // Guest animation controllers (one per chair, keyed by furniture ID)
  std::unordered_map<core::FurnitureId,
                     views::screens::restaurant::CharacterController,
                     core::FurnitureId::Hasher>
      guest_controllers_;

  int furniture_draw_counter_{0};
  int interior_end_row_{0};
  int top_rows_consumed_{0};
  int interior_start_row_{0};
  std::vector<FurnitureBatch>& furniture_placement_;
  // index into furniture_placement_:
  std::optional<size_t> active_batch_index_;
  // Where to navigate after placement complete:
  std::optional<SupplierType> return_to_supplier_type_;
  std::optional<audio::SoundId> current_restaurant_noise_;
  StatusController status_controller_;
  input::ListenerToken listener_;
  // For weapon and sabotage picker:
  std::optional<core::item::Item> hovered_item_;
  RmbDragScroller rmb_drag_scroller_;

  void seat_arrived_guest();

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

  // For attacking competitors
  std::optional<AttackSprite> attacker_;
  // Only if player 2 is a human
  std::optional<AttackSprite> defender_;
  bool attack_active_{false};
  // Weapon for attacking (TODO: consume on use when we actually implement that)
  core::item::Item attack_weapon_{};
};
