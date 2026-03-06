/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <map>
#include <vector>

#include "core/city/CarManager.h"
#include "core/city/CityTypes.h"
#include "engine/geom/Camera.h"
#include "engine/geom/Spaces.h"
#include "gfx/PaletteCycleController.h"
#include "views/base/MainPanelView.h"
#include "views/helpers/camera/CameraController.h"
#include "views/helpers/camera/ScrollEdges.h"
#include "views/screens/city_view/CityViewCommon.h"
#include "views/screens/city_view/CityViewMapState.h"
#include "views/screens/city_view/CityViewTab.h"
#include "views/ui/DropdownManager.h"
#include "views/ui/status/StatusController.h"

class City;

class CityView final : public MainPanelView {
public:
  CityView(GameContext& context, GameState& state, core::CityId id);
  ~CityView();
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  enum class Dropdown { PLAYER, RANGE, LOCATIONS, PROPERTIES };

  // Zooming
  void zoom_in(ViewPoint p);
  void zoom_out(ViewPoint p);
  void on_zoom_close();
  void on_zoom_medium();
  void on_zoom_far();
  void on_zoom_not_close();
  void apply_zoom_profile(ZoomLevel zoom);
  void enable_zoom_cursor(ViewPoint p);
  void disable_zoom_cursor();
  void handle_mouse_click_zoom_close(WorldPoint p);
  static Surface create_zoom_box_cursor(int width, int height,
                                        int border_width);
  bool is_valid_zoom_box_location(ViewPoint p);

  // Rendering
  void render_districts(const RenderContext& ctx);
  void render_control(const RenderContext& ctx);
  void render_with_color(Surface& map,
                         std::function<uint32_t(core::DistrictId)> color_fn);
  void render_with_cell_color(Surface& map,
                              std::function<uint8_t(int)> color_fn);
  void render_map_data(const RenderContext& ctx);
  void render_map_zoom_close(const RenderContext& ctx);
  void render_map_zoom_medium(const RenderContext& ctx);
  void render_map_zoom_far(const RenderContext& ctx);
  void draw_cell_with_pattern(Surface& map, ViewRect cell, uint8_t color,
                              WorldPoint cell_world_origin,
                              ViewPoint cell_screen_origin);

  // Translation
  int index_at(WorldPoint p) const;
  int index_by_x_y_offset(WorldPoint p, int x_offset, int y_offset) const;
  bool index_is_in_view(int, int, int) const;
  WorldPoint world_pos_of_index(int index) const;

  // Tabs
  void show_tabs(const RenderContext& ctx);
  void render_tabs(const RenderContext& ctx);
  void update_tab_positions();

  // One off initialisation methods
  void create_dropdowns();
  void create_buttons();
  void register_map_interactions_viewport();

  // Helper for coordinate conversions
  ViewPoint local_to_viewport_coords(ViewPoint local_pos) const;

  //  Highlights:
  void highlight_if(std::function<HighlightType(int)> predicate);
  void render_highlights(const RenderContext& ctx);
  void clear_highlights();
  void highlight_business_premises();
  void highlight_building_type(BuildingType);
  void highlight_player(core::PlayerId player_id, bool range);
  void highlight_properties(PremiseStatus status);

  // District
  core::DistrictId district_at(WorldPoint p);

  // State
  bool restore_state_from_city();
  void store_state_in_city(CityViewReentryMode entry_mode);
  CityViewMapState capture_view_state() const;
  void restore_view_state(const CityViewMapState& state);
  void current_highlight(CityViewMapState);
  void highlight_mode(const CityViewHighlight& highlight);
  template <typename T, typename... Args>
  void ui_push_store_position(CityViewReentryMode mode, Args&&... args);

  // Animation
  void update_water_palette(const RenderContext& ctx);
  void setup_water_animation();
  void move_cars();
  void place_cars();

  // Status display
  void build_default_city_status_recipe();
  void build_property_status_recipe(int clicked_index);

  const Surface& tiles_medium_;
  City* city_{nullptr};
  ZoomLevel zoom_;
  bool zoom_selector_;
  bool show_cell_borders_;
  bool show_cell_type_;
  std::map<uint8_t, uint32_t> zoom_far_colors_;
  int active_tab_;
  // Current tabs with their positions and states.
  std::vector<CityViewTab> tabs_;
  // The currently active tab.
  CityViewTabType active_tab_type_;
  bool tabs_changed_ = true;
  CityViewHighlight highlight_mode_{};
  std::vector<HighlightType> highlighted_cells_;
  // User has requested a highlight:
  bool highlight_enabled_ = false;
  // Highlight is active on this specific frame:
  bool highlight_blink_visible_ = false;
  std::unordered_map<core::DistrictId, uint8_t, core::DistrictId::Hasher>
      bezid_to_color_map_;
  bool color_map_initialized_ = false;
  Bob& stanbobs_;
  Surface zoom_box_far_;
  Surface zoom_box_medium_;
  std::optional<CarManager> car_mgr_;
  DropdownManager<Dropdown> dropdowns_;
  std::vector<uint8_t> water_indices_;
  std::vector<std::vector<uint8_t>> water_cycles_;
  PaletteCycleController water_animation_;
  int property_display_timer_ = 0;
  StatusController status_controller_;
  Camera camera_;
  CameraController camctrl_;
  ScrollEdges scroll_edges_{camctrl_, wood_edge_rects(panel_origin_)};
  gfx::PaletteOverride water_ovr_;
  std::vector<gfx::Color> water_buf_;
};
