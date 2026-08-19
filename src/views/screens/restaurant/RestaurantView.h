/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/item/Item.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/ClockRate.h"
#include "core/state/InterruptionRate.h"
#include "core/state/UIState.h"
#include "engine/audio/AudioManager.h"
#include "engine/geom/Camera.h"
#include "engine/geom/Spaces.h"
#include "views/base/BobRenderer.h"
#include "views/base/MainPanelView.h"
#include "views/helpers/camera/CameraController.h"
#include "views/helpers/camera/RmbDragScroller.h"
#include "views/helpers/camera/ScrollEdges.h"
#include "views/screens/restaurant/AttackScene.h"
#include "views/screens/restaurant/CharacterController.h"
#include "views/screens/restaurant/FurniturePlacementController.h"
#include "views/screens/restaurant/GuestWalkScene.h"
#include "views/screens/restaurant/SeatedCharacterRenderer.h"
#include "views/screens/restaurant/WalkNavigation.h"
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
  InterruptionRate interruption_rate() const override;
  ClockRate clock_rate() const override;

private:
  void handle_mouse_click(WorldPoint map_point);
  // True only when viewing the player's own restaurant
  bool in_player_mode() const {
    return view_mode_ == RestaurantViewMode::PLAYER_DEFAULT ||
           view_mode_ == RestaurantViewMode::PLAYER_NEW_FURNITURE;
  }
  void toggle_kitchen_view();
  void apply_view_camera();
  void calculate_background_dimensions();
  void set_up_buttons();

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
  const BobRenderer& bob_renderer_for(const PlacedFurniture& pf) const;
  const std::vector<std::unique_ptr<PlacedFurniture>>&
  current_furniture_list() const;
  void handle_furniture_click(const PlacedFurniture* pf);
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

  void draw_floor_tiles(ViewRect interior);

  template <class Pred> void render_furniture_where(Pred&& pred);

  void render_placement_layer(PlacementLayer layer);
  void
  render_furniture_excluding(std::initializer_list<FurnitureCategory> cats);

  void render_guest_legs();
  void render_guest_upper();
  void render_guest_pizzas();

  void rebuild_guest_cache();
  static ViewPoint center_of(const ViewRect& r);

  static std::optional<ViewRect> map_rect_to_viewport(const WorldRect& r_map,
                                                      const Camera& camera);
  void render_top_wall_and_floor();
  void render_side_and_bottom_walls();

  void zoom_out_of_restaurant_view();
  void update_status_display(const PlacedFurniture* pf);
  void update_restaurant_noise();
  // Cached navigation grid for the dining room (see WalkNavigation.h);
  // invalidated whenever furniture changes.
  [[nodiscard]] const views::screens::restaurant::NavGrid&
  navigation_grid() const;
  void invalidate_nav_grid() { nav_grid_cache_.reset(); }

  const Surface& k0_;
  bool kitchen_view_ = false;
  bool closed_for_placement_ = false;
  mutable std::optional<views::screens::restaurant::NavGrid> nav_grid_cache_;
  int background_width_ = 0;
  int background_height_ = 0;
  int furniture_offset_x_ = 0; // for kitchen
  int furniture_offset_y_ = 0; // for restaurants
  const BobRenderer furniture_renderer_;
  const BobRenderer oven_renderer_;
  Bob& pizza_bob_;
  Restaurant* restaurant_;
  Surface background_;
  std::optional<core::FurnitureId> last_clicked_furniture_index_;
  RestaurantViewMode view_mode_;
  Camera camera_;

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
  std::optional<audio::SoundId> current_restaurant_noise_;
  StatusController status_controller_;
  input::ListenerToken listener_;
  // For weapon and sabotage picker:
  std::optional<core::item::Item> hovered_item_;
  RmbDragScroller rmb_drag_scroller_;

  // Friend scenes so they can access things like the furniture list, etc.
  friend class views::screens::restaurant::AttackScene;
  friend class views::screens::restaurant::GuestWalkScene;
  friend class views::screens::restaurant::FurniturePlacementController;
  views::screens::restaurant::FurniturePlacementController placement_{
      *this, context_, state_};
  views::screens::restaurant::GuestWalkScene walk_scene_{*this, context_,
                                                         state_};
  views::screens::restaurant::AttackScene attack_{*this, context_, state_};
};
