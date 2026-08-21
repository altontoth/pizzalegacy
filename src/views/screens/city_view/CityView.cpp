/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/city_view/CityView.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include "conversations/MafiaLine.h"
#include "core/GameTypes.h"
#include "core/character/MafiaPlayerState.h"
#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/CityTypes.h"
#include "core/city/CityViewCommon.h"
#include "core/city/CityViewMapState.h"
#include "core/city/District.h"
#include "core/city/Property.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/RestaurantTypes.h"
#include "core/state/TurnOrchestrator.h"
#include "core/time/GameTime.h"
#include "core/util/Constants.h"
#include "engine/Random.h"
#include "engine/audio/AudioManager.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/ICursorManager.h"
#include "gfx/Draw.h"
#include "gfx/Surface.h"
#include "views/base/ViewDefs.h"
#include "views/screens/StatusDisplay.h"
#include "views/screens/city_view/ZoomConfig.h"
#include "views/ui/popups/ConfirmPopup.h"

constexpr int DEV_RES_W = 18;
constexpr int DEV_RES_H = 18;
constexpr int DEV_KTCH_H = 21;
constexpr int DEV_KTCH_W = 21;
constexpr int DISTRICT_COLORS_INDEX = 0;

// When zooming in from FAR to MEDIUM or MEDIUM to CLOSE you always zoom_ this
// much:
constexpr int ZOOM_FACTOR = 4;

// Palette indices 28-31 are grass colors used at all zoom levels.
// The original overrides these with per-season RGB values (6-bit vga).
constexpr std::array<uint8_t, 4> SEASON_INDICES = {28, 29, 30, 31};

// RGB values per season for indices 28-31.
// Each row: index 28, 29, 30, 31, each entry is rgb
constexpr std::array<std::array<std::array<uint8_t, 3>, 4>, 4> SEASON_COLORS = {
    {
        // Spring -- base green
        {{{0, 12, 0}, {12, 24, 0}, {12, 20, 0}, {16, 16, 0}}},
        // Summer --  green/yellow-ish
        {{{4, 12, 0}, {36, 24, 0}, {24, 20, 0}, {20, 16, 0}}},
        // Autumn -- brown-ish
        {{{16, 12, 0}, {36, 24, 12}, {28, 20, 4}, {24, 16, 4}}},
        // Winter -- blue-gray
        {{{12, 12, 16}, {44, 36, 44}, {36, 32, 36}, {16, 24, 24}}},
    }};

constexpr PaletteHandle palette_for_season(Season season) {
  switch (season) {
  case Season::Spring:
    return PaletteHandle{0};
  case Season::Summer:
    return PaletteHandle{0x43};
  case Season::Fall:
    return PaletteHandle{0x44};
  case Season::Winter:
    return PaletteHandle{0x45};
  }
  return PaletteHandle{0};
}

using namespace views::base;
using namespace LegacyConstants;

CityView::CityView(GameContext& context, GameState& state, core::CityId id)
    : MainPanelView(context, state),
      tiles_medium_(context_.vga->get("gfx/med1")), zoom_(ZoomLevel::FAR),
      zoom_selector_(false), show_cell_borders_(false), show_cell_type_(false),
      stanbobs_(context_.bob->get("bob/stanbobs.bob")),
      zoom_box_far_(create_zoom_box_cursor(
          ZOOM_BOX_WIDTH_FAR, ZOOM_BOX_HEIGHT_FAR, ZOOM_BOX_BORDER_WIDTH)),
      zoom_box_medium_(create_zoom_box_cursor(ZOOM_BOX_WIDTH_MEDIUM,
                                              ZOOM_BOX_HEIGHT_MEDIUM,
                                              ZOOM_BOX_BORDER_WIDTH)),
      palette_(palette_for_season(state.time().season())),
      status_controller_(state.status()), camctrl_{context.input, camera_} {

  state_.active_city(id);
  city_ = state_.active_city();

  if (auto* player = state_.active_player()) {
    state_.on_city_entered(id, *player);
  }

  if (state_.side_menu_charge_pending() &&
      !state_.turn().is_processing_end_of_turn()) {
    state_.side_menu_charge_pending(false);
    state_.advance_time_blocks(1);
  }

  for (int i = 0; i < city_->data_length(); ++i) {
    highlighted_cells_.push_back(HighlightType::NONE);
  }

  // Initialize camera world size BEFORE restore_view_state so scroll can be
  // restored correctly
  const int world_tile_w = BASE_MAP_TILE_WIDTH * ZOOM_FACTOR;
  const int world_tile_h = BASE_MAP_TILE_HEIGHT * ZOOM_FACTOR;
  camera_.world_size(Size{MAP_TILES_PER_ROW * world_tile_w,
                          MAP_TILES_PER_COLUMN * world_tile_h});

  // Apply zoom profile to initialize camera view_size and zoom BEFORE
  // restore_view_state, so the camera is properly configured even on first load
  apply_zoom_profile(zoom_);

  create_buttons();

  constexpr int tab_width = 104;
  constexpr int tab_height = 23;
  tabs_.emplace_back(CityViewTabType::CONTROL,
                     ViewRect{0, 0, tab_width, tab_height}, 0);
  tabs_.emplace_back(CityViewTabType::DISTRICTS,
                     ViewRect{0, 0, tab_width, tab_height}, 1);
  tabs_.emplace_back(CityViewTabType::MAP,
                     ViewRect{0, 0, tab_width, tab_height}, 2);

  restore_view_state(state_.city_view_view_state());

  create_dropdowns();

  scroll_edges_.add_interactions(interactions_);

  using namespace views::base;
  const ViewRect panel{LegacyConstants::WOOD_BORDER_WIDTH_LEFT,
                       LegacyConstants::WOOD_BORDER_HEIGHT_TOP,
                       MAIN_PANEL_WIDTH, MAIN_PANEL_HEIGHT};
  rmb_drag_scroller_.init(
      camctrl_,
      [this] {
        return context_.config->options().qol_improvements && !zoom_selector_;
      },
      [this] { return context_.config->options().rmb_drag_inverted; },
      [this](ViewPoint abs_pos) {
        const auto cfg = ZoomConfig::for_zoom_level(zoom_);
        ViewPoint vp{abs_pos.x - cfg.x_borders_offset,
                     abs_pos.y - cfg.y_borders_offset};
        if (zoom_ == ZoomLevel::CLOSE || zoom_ == ZoomLevel::MEDIUM) {
          zoom_out(vp);
        }
      },
      [this](bool lock) { context_.cursor->set_relative_mouse_mode(lock); });
  rmb_drag_scroller_.add_interaction(interactions_, panel,
                                     [this] { mark_dirty(); });

  // Build season-adjusted palette: start from the seasonal base palette,
  // then override indices 28-31 with per-season vegetation colors.
  // VGA values are 6-bit (0-63), shift left 2 to get 8-bit (0-252).
  season_palette_ = gfx::PaletteBank::instance().colors(palette_);
  int season_idx = static_cast<int>(state.time().season());
  for (int i = 0; i < 4; ++i) {
    const auto& rgb = SEASON_COLORS[season_idx][i];
    season_palette_[SEASON_INDICES[i]] = {
        .r = static_cast<uint8_t>(rgb[0] << 2),
        .g = static_cast<uint8_t>(rgb[1] << 2),
        .b = static_cast<uint8_t>(rgb[2] << 2),
    };
  }

  setup_water_animation();
  status_controller_.show_city_default({.state = &state_, .city = city_});
}

CityView::~CityView() {
  if (should_save_map_state_on_exit_) {
    store_state_in_city(CityViewReentryMode::RESTORE_ALL);
  }
  state_.clear_active_district();
  stop_city_sound();
}

template <typename T, typename... Args>
void CityView::ui_push_store_position(CityViewReentryMode mode,
                                      Args&&... args) {
  store_state_in_city(mode);
  state_.ui().push<T>(std::forward<Args>(args)...);
}

void CityView::register_map_interactions_viewport() {
  // one full-viewport item, rebuilt every frame
  ZoomConfig zoom = ZoomConfig::for_zoom_level(zoom_);
  const ViewRect viewport{zoom.x_borders_offset, zoom.y_borders_offset,
                          zoom.viewport_width, zoom.viewport_height};

  interactions_.create_frame_item(
      viewport, "citymap_viewport",
      [this](const views::ui::InteractionEvent& ie) {
        switch (ie.type) {

        // District hovering status:
        case views::ui::InteractionEvent::Type::Hover:
        case views::ui::InteractionEvent::Type::Enter: {
          if (zoom_ == ZoomLevel::MEDIUM || zoom_ == ZoomLevel::FAR) {
            // Convert screen coordinates to viewport coordinates accounting for
            // offset
            const auto viewport_coords = camera_.view_to_world(ie.local);
            const auto did = district_at(viewport_coords);
            if (did != state_.active_district_id()) {
              state_.active_district_id(did);
              build_default_city_status_recipe();
              context_.status_panel->mark_dirty();
              mark_dirty();
            }

            // is_valid_zoom_box_location expects local coordinates
            if (zoom_selector_ && !is_valid_zoom_box_location(ie.local)) {
              disable_zoom_cursor();
              mark_dirty();
              break;
            }
          }
          break;
        }
        case views::ui::InteractionEvent::Type::Leave:
          disable_zoom_cursor();
          break;
        // Zoom in or selection of tiles:
        case views::ui::InteractionEvent::Type::ButtonDown: {
          if (ie.button == views::ui::MouseButton::Left) {
            if (zoom_ == ZoomLevel::CLOSE) {
              handle_mouse_click_zoom_close(camera_.view_to_world(ie.local));
              mark_dirty();
              break;
            }
            if (zoom_selector_) {
              zoom_in(ie.local);
            }
            else {
              enable_zoom_cursor(ie.local);
            }
            mark_dirty();
          }
          else if (ie.button == views::ui::MouseButton::Right) {
            if (zoom_selector_) {
              disable_zoom_cursor();
            }
            else if (!context_.config->options().qol_improvements) {
              if (zoom_ == ZoomLevel::CLOSE || zoom_ == ZoomLevel::MEDIUM) {
                zoom_out(ie.local);
              }
            }
            mark_dirty();
          }
          break;
        }
        case views::ui::InteractionEvent::Type::Scroll: {
          if (!context_.config->options().qol_improvements) {
            break;
          }
          if (ie.scroll_delta > 0 && zoom_ != ZoomLevel::CLOSE) {
            zoom_scroll(1, ie.local);
          }
          else if (ie.scroll_delta < 0 && zoom_ != ZoomLevel::FAR) {
            zoom_scroll(-1, ie.local);
          }
          break;
        }
        default:
          break;
        }
      });
}

void CityView::disable_zoom_cursor() {
  zoom_selector_ = false;
  context_.cursor->clear_custom_cursor();
}

ViewPoint CityView::local_to_viewport_coords(ViewPoint local_pos) const {
  // Convert local coordinates (relative to the interaction item) to viewport
  // coordinates by accounting for the viewport offset on screen.
  const auto z = ZoomConfig::for_zoom_level(zoom_);
  return ViewPoint{local_pos.x + z.x_borders_offset,
                   local_pos.y + z.y_borders_offset};
}

void CityView::zoom_in(ViewPoint p_view_local) {
  const auto from = ZoomConfig::for_zoom_level(zoom_);
  const ZoomLevel new_zoom =
      (zoom_ == ZoomLevel::MEDIUM) ? ZoomLevel::CLOSE : ZoomLevel::MEDIUM;
  const auto to = ZoomConfig::for_zoom_level(new_zoom);

  // Anchor: the world point under the cursor *before* zoom change
  const WorldPoint anchor_world = camera_.view_to_world(p_view_local);

  // Commit camera + controller config first to update zoom
  zoom_ = new_zoom;
  disable_zoom_cursor();
  apply_zoom_profile(zoom_);

  camera_.scroll(anchor_world);
  camctrl_.reset();

  mark_dirty();
}

void CityView::update_city_sound() {
  if (!context_.audio || !context_.config->options().map_fx) {
    return;
  }

  audio::SoundId desired;
  if (zoom_ == ZoomLevel::CLOSE) {
    desired = audio::SoundId::TRAFFIC_1;
  }
  else if (zoom_ == ZoomLevel::MEDIUM) {
    desired = audio::SoundId::TRAFFIC_2;
  }
  else {
    stop_city_sound();
    return;
  }

  if (current_city_sound_ && *current_city_sound_ == desired) {
    return;
  }

  stop_city_sound();
  current_city_sound_ = desired;
  context_.audio->play_sound(desired, audio::LoopMode::Loop);
}

void CityView::stop_city_sound() {
  if (context_.audio && current_city_sound_) {
    context_.audio->stop_sound(*current_city_sound_);
    current_city_sound_.reset();
  }
}

void CityView::on_zoom_close() {
  water_animation_.request_start();
  if (!car_mgr_) {
    car_mgr_.emplace(context_.bob, *city_);
  }
  update_city_sound();
}

void CityView::on_zoom_medium() {
  update_city_sound();
  on_zoom_not_close();
}

void CityView::on_zoom_far() {
  update_city_sound();
  on_zoom_not_close();
}

void CityView::on_zoom_not_close() {
  water_animation_.request_stop();
  car_mgr_.reset();
}

void CityView::enable_zoom_cursor(ViewPoint p) {
  if (is_valid_zoom_box_location(p)) {
    zoom_selector_ = true;
    context_.cursor->custom_cursor(
        zoom_ == ZoomLevel::MEDIUM ? zoom_box_medium_ : zoom_box_far_, false);
  }
}

void CityView::zoom_out(ViewPoint p_view_local) {
  const auto from = ZoomConfig::for_zoom_level(zoom_);
  const ZoomLevel new_zoom =
      (zoom_ == ZoomLevel::CLOSE) ? ZoomLevel::MEDIUM : ZoomLevel::FAR;
  const auto to = ZoomConfig::for_zoom_level(new_zoom);

  // Anchor: the world point under the cursor *before* zoom change
  const WorldPoint anchor_world = camera_.view_to_world(p_view_local);

  // Commit zoom level and apply camera profile
  zoom_ = new_zoom;
  apply_zoom_profile(zoom_);

  // Calculate new scroll position based on new camera state
  int new_x = 0;
  int new_y = 0;

  if (new_zoom == ZoomLevel::MEDIUM) {
    // Reset any status display that was about close zoom level things
    // that are now not visible in detail anymore.
    state_.status().clear_overlay();
    // Keep the same world point under the *center* of the new view if possible.
    const ViewPoint o = camera_.origin_offset_in_view();
    const ViewPoint center_view{to.viewport_width / 2, to.viewport_height / 2};

    // We want: center_view = (anchor_world - new_scroll) / new_zoom + o
    // Solving: new_scroll = anchor_world - (center_view - o) * new_zoom
    const int unaligned_x =
        anchor_world.x - ((center_view.x - o.x) * to.zoom_multiplier);
    const int unaligned_y =
        anchor_world.y - ((center_view.y - o.y) * to.zoom_multiplier);

    // Snap to MEDIUM tile grid (world px). Let camera clamp on scroll.
    new_x = Camera::snap_to_tile(unaligned_x, to.tile_width);
    new_y = Camera::snap_to_tile(unaligned_y, to.tile_height);
  }
  // FAR: leave scrolling at 0,0

  // Set scroll (camera will clamp based on zoom and world_size)
  camera_.scroll(WorldPoint{new_x, new_y});
  camctrl_.reset();

  mark_dirty();
}

void CityView::zoom_scroll(int direction, ViewPoint cursor) {
  ZoomLevel new_zoom;
  if (direction > 0) {
    new_zoom =
        (zoom_ == ZoomLevel::MEDIUM) ? ZoomLevel::CLOSE : ZoomLevel::MEDIUM;
  }
  else {
    new_zoom = (zoom_ == ZoomLevel::CLOSE) ? ZoomLevel::MEDIUM : ZoomLevel::FAR;
  }
  const auto to = ZoomConfig::for_zoom_level(new_zoom);

  const WorldPoint anchor_world = camera_.view_to_world(cursor);

  if (new_zoom == ZoomLevel::MEDIUM) {
    state_.status().clear_overlay();
  }

  zoom_ = new_zoom;
  disable_zoom_cursor();
  apply_zoom_profile(zoom_);

  // Keep anchor_world under cursor after the zoom change.
  // Derived from view_to_world: world = (view - o) * zoom + scroll
  // => new_scroll = anchor_world - (cursor - o) * new_zoom
  const ViewPoint o = camera_.origin_offset_in_view();
  camera_.scroll(
      WorldPoint{anchor_world.x - ((cursor.x - o.x) * to.zoom_multiplier),
                 anchor_world.y - ((cursor.y - o.y) * to.zoom_multiplier)});
  camctrl_.reset();

  mark_dirty();
}

void CityView::apply_zoom_profile(ZoomLevel zoom) {
  const auto cfg = ZoomConfig::for_zoom_level(zoom);

  camera_.view_size(Size{cfg.viewport_width, cfg.viewport_height});
  camera_.zoom(cfg.zoom_multiplier);

  camctrl_.tile_grid(cfg.tile_width * cfg.zoom_multiplier,
                     cfg.tile_height * cfg.zoom_multiplier);
  camctrl_.speed_multipliers(cfg.scroll_speed_multiplier,
                             cfg.scroll_speed_multiplier * 4.0f);
  camctrl_.reset();

  if (zoom == ZoomLevel::CLOSE) {
    on_zoom_close();
  }
  else if (zoom == ZoomLevel::MEDIUM) {
    on_zoom_medium();
  }
  else {
    on_zoom_far();
  }
}

void CityView::handle_mouse_click_zoom_close(WorldPoint p) {
  // Based on the current viewport and where we clicked, we can find out which
  // building was clicked.
  int clicked_index = index_at(p);
  Logger::debug("Click at index {}, tile type 0x{:02x}", clicked_index,
                city_->type(clicked_index));
  //  If you click on the right side of a depot, you transparently get
  //  directed as if you clicked the left side. So that there is only one
  //  tile that is actually the depot, but you'll never know!
  if (city_->type(clicked_index) == 0x14) {
    clicked_index--;
    Logger::debug("  Adjusted to index {} (depot right side)", clicked_index);
  }

  // Check for active mafia mission at this location and time
  auto& mafia = state_.active_player()->mafia_state();
  if (mafia.active_mission && mafia.active_mission->active &&
      !mafia.active_mission->completed) {
    auto& mission = *mafia.active_mission;
    const auto& time = state_.time();

    bool at_pickup = !mission.pickup_done &&
                     mission.pickup.tile_index == clicked_index &&
                     mission.pickup.is_active(time);
    bool at_dropoff = mission.dropoff.tile_index == clicked_index &&
                      mission.dropoff.is_active(time);

    Logger::debug(
        "Mafia mission check: pickup tile={} (done={}) dropoff tile={} "
        "clicked={} at_pickup={} at_dropoff={}",
        mission.pickup.tile_index, mission.pickup_done,
        mission.dropoff.tile_index, clicked_index, at_pickup, at_dropoff);

    if (at_pickup || at_dropoff) {
      if (mission.busted) {
        // Pre-rolled bust: police arrest instead of mission success.
        // Crime code = mission type (Package=1, Weapon=3, Laundering=4, etc.)
        auto crime = static_cast<ArrestReason>(
            static_cast<int>(mission.mission_type) + 1);
        // Social penalty fires before police: -(crime_code x 10)
        if (auto* player = state_.active_player()) {
          state_.adjust_social_score(*player, -static_cast<int>(crime) * 10);
        }
        mafia.active_mission = std::nullopt;
        state_.initiate_arrest(crime);
        return;
      }

      if (at_pickup) {
        state_.ui().push<MafiaMissionViewState>(
            MafiaLine::SSSHHH_HERE_TAKE_THIS_SUITCASE);
      }
      else {
        state_.ui().push<MafiaMissionViewState>(
            MafiaLine::COME_ON_HAND_IT_OVER);
      }
      return;
    }
  }

  auto* property = city_->property_by_index(clicked_index);
  bool is_business = city_->is_business_premise_by_index(clicked_index);
  bool is_depot = property && property->type() == PropertyType::DEPOT;
  auto last_clicked = state_.last_clicked_property_index();

  Logger::debug(
      "  is_business_premise={}, is_depot={}, last_clicked={}, property={}",
      is_business, is_depot, last_clicked.value_or(-1),
      property ? "exists" : "null");

  if (property) {
    Logger::debug(
        "  property type={}, owner={}", static_cast<int>(property->type()),
        property->owner_or_tenant() ? property->owner_or_tenant()->value : -1);
  }

  if ((is_business || is_depot) && last_clicked == clicked_index) {
    auto* current_player = state_.active_player();

    // Only enter restaurant if owned by current player, TODO deal with
    // competitors
    if (property && property->type() == PropertyType::BUSINESS_PREMISE &&
        property->has_restaurant()) {
      auto r_id = property->restaurant_id();
      state_.active_restaurant(*r_id);

      RestaurantViewMode map_mode;
      if (current_player->id() == *property->owner_or_tenant()) {
        map_mode = RestaurantViewMode::PLAYER_DEFAULT;
      }
      else {
        map_mode = RestaurantViewMode::OPPONENT_VIEW;
      }

      state_.ui().push<RestaurantViewState>(*r_id, map_mode);
      return;
    }
    // Enter depot if owned/rented by current player
    if (property && property->type() == PropertyType::DEPOT) {
      auto owner = property->owner_or_tenant();
      Logger::debug("  Depot check: owner={}, current_player={}",
                    owner ? owner->value : -1, current_player->id().value);
      if (owner && *owner == current_player->id()) {
        Logger::debug("  Pushing DepotMainState!");
        state_.ui().push<DepotMainState>(property);
        return;
      }
    }
  }

  state_.last_clicked_property_index(clicked_index);
  auto district_id = district_at(p);
  build_property_status_recipe(clicked_index);
  state_.active_district_id(district_id);
}

void CityView::tick() {
  if (camctrl_.tick()) {
    // During scrolling, we just stop everything:
    mark_dirty();
    return;
  }
  water_animation_.update();
  if (zoom_ == ZoomLevel::CLOSE) {
    WorldRect view = camera_.visible_world_rect();
    car_mgr_->update(view);
    mark_dirty();
  }
  else {
    if (dropdowns_.tick()) {
      mark_dirty();
    }
  }

  if (zoom_ != ZoomLevel::CLOSE && highlight_enabled_) {
    const uint64_t now = context_.clock.now_ms;
    const bool new_blink_status = ((now / 500) % 2) == 0; // 500 ms period
    if (new_blink_status != highlight_blink_visible_) {
      highlight_blink_visible_ = new_blink_status;
      mark_dirty();
    }
  }
}

void CityView::render(const RenderContext& ctx) {
  ctx.surface->colors(season_palette_);
  // Let all dropdowns register their interactions
  interactions_.begin_frame();

  register_map_interactions_viewport();

  if (zoom_ == ZoomLevel::CLOSE) {
    // Always update water palette when animation is active since Renderer
    // clears it every frame
    if (water_animation_.is_active()) {
      update_water_palette(ctx);
    }

    render_map_data(ctx);
    car_mgr_->render(ctx, camera_);
  }
  else {
    render_tabs(ctx);
    if (active_tab_type_ == CityViewTabType::DISTRICTS) {
      render_districts(ctx);
    }
    else if (active_tab_type_ == CityViewTabType::CONTROL) {
      render_control(ctx);
    }
    else {
      render_map_data(ctx);
    }
  }
  if (zoom_ != ZoomLevel::CLOSE) {
    if (highlight_blink_visible_) {
      render_highlights(ctx);
    }

    views::ui::InteractionContext ictx{.layers = interactions_,
                                       .invalidate = [this] { mark_dirty(); }};
    dropdowns_.add_interactions(ictx);
    dropdowns_.render(*ctx.surface, context_.font);
  }
}

void CityView::render_map_zoom_close(const RenderContext& ctx) {
  ctx.surface->fill(240);

  // zoom/tiles
  const uint8_t multiplier = BASE_MAP_ZOOM_MULTIPLIER;
  const int tile_w = BASE_MAP_TILE_WIDTH * multiplier;
  const int tile_h = BASE_MAP_TILE_HEIGHT * multiplier;

  const auto visible = camera_.visible_world_rect();

  const int first_tx = std::clamp(visible.x / tile_w, 0, MAP_TILES_PER_ROW - 1);
  const int first_ty =
      std::clamp(visible.y / tile_h, 0, MAP_TILES_PER_COLUMN - 1);

  // pixel offset inside the first tile
  const int off_x = -(visible.x % tile_w);
  const int off_y = -(visible.y % tile_h);

  // how many tiles to cover the panel (+2 to cover partials on right/bottom)
  const int tiles_x = (visible.w / tile_w) + 2;
  const int tiles_y = (visible.h / tile_h) + 2;

  const auto& landsym = context_.vga->get("gfx/landsym");

  for (int ty = 0; ty < tiles_y; ++ty) {
    const int map_y = first_ty + ty;
    if (map_y >= MAP_TILES_PER_COLUMN) {
      break;
    }

    for (int tx = 0; tx < tiles_x; ++tx) {
      const int map_x = first_tx + tx;
      if (map_x >= MAP_TILES_PER_ROW) {
        break;
      }

      const int i = (map_y * MAP_TILES_PER_ROW) + map_x;
      if (i >= city_->data_length()) {
        continue;
      }

      const uint8_t type = city_->type(i);
      const ViewRect src = landsym_source_rect(type, multiplier);

      // Convert world tile position to view space for rendering
      const WorldPoint tile_world_pos = {map_x * tile_w, map_y * tile_h};
      const ViewPoint dest_pos = camera_.world_to_view(tile_world_pos);

      ctx.surface->blit(landsym, src, dest_pos);

      if (show_cell_borders_) {
        if (i >= 0 && i < 1'000'000) {
          context_.font->render_text_onto(*ctx.surface, std::to_string(i),
                                          {dest_pos.x + 3, dest_pos.y + 4},
                                          FontFace::MINI, 255);
        }
      }

      if (show_cell_type_) {
        char road_type_str[3];
        snprintf(road_type_str, 3, "%02x", type);
        auto roadTypeOpt = magic_enum::enum_cast<RoadType>(type);
        auto color = roadTypeOpt.has_value() ? 255 : 32;
        context_.font->render_text_onto(*ctx.surface, road_type_str,
                                        {dest_pos.x + 3, dest_pos.y + 12},
                                        FontFace::MINI, color);
      }
    }
  }
}

void CityView::render_map_zoom_medium(const RenderContext& ctx) {
  // World is always in close scale (32-pixel tiles = 5120x3840 world space)
  // But at medium we render with smaller tile graphics (8x8 from the source)
  if (!medium_map_cache_) {
    Surface medium_map({MAP_TILES_PER_ROW * BASE_MAP_TILE_WIDTH,
                        MAP_TILES_PER_COLUMN * BASE_MAP_TILE_HEIGHT},
                       palette_);
    for (int i = 0; i < city_->data_length(); ++i) {
      const uint8_t type = city_->type(i);
      const ViewPoint pos{(i % MAP_TILES_PER_ROW) * BASE_MAP_TILE_WIDTH,
                          (i / MAP_TILES_PER_ROW) * BASE_MAP_TILE_HEIGHT};
      medium_map.blit(tiles_medium_, landsym_source_rect(type), pos);
    }
    medium_map_cache_ = std::move(medium_map);
  }

  // One cache pixel covers zoom_multiplier world pixels.
  const auto visible = camera_.visible_world_rect();
  const ViewRect src{visible.x / BASE_MAP_ZOOM_MULTIPLIER,
                     visible.y / BASE_MAP_ZOOM_MULTIPLIER,
                     visible.w / BASE_MAP_ZOOM_MULTIPLIER,
                     visible.h / BASE_MAP_ZOOM_MULTIPLIER};
  const PxOffset tab_offset{TABBED_VIEW_OFFSET_X, TABBED_VIEW_OFFSET_Y};
  const ViewPoint dest =
      camera_.world_to_view(WorldPoint{visible.x, visible.y}) + tab_offset;
  ctx.surface->blit(*medium_map_cache_, src, dest);
}

void CityView::render_map_zoom_far(const RenderContext& ctx) {
  if (!far_map_cache_) {
    Surface far_map({TABBED_VIEWPORT_WIDTH, TABBED_VIEWPORT_HEIGHT}, palette_);
    // This is basically landsym but then at one pixel per tile, hardcoded:
    constexpr std::array<uint8_t, 256> palette_map{
        0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1E, 0x1E, 0x1E, 0x1C, 0x1E,
        0x1E, 0x1E, 0x1E, 0x1E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E,
        0x0E, 0x1D, 0x1D, 0x1D, 0x13, 0x1C, 0x1D, 0x1D, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
        0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x1A,
        0x1A, 0x1A, 0x1A, 0x1B, 0x0B, 0x0B, 0x15, 0x15, 0x15, 0x15, 0x0C, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0D, 0x0D, 0x0D, 0x0D,
        0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D,
        0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
        0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
        0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x08, 0x08, 0x08, 0x08,
        0x08, 0x08, 0x08, 0x08, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E,
        0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x1F, 0x1F, 0x1F, 0x1F,
        0x1F, 0x1F, 0x1F, 0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
        0x11, 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
        0x15, 0x15, 0x15, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x10, 0x10, 0x10, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
        0x0C, 0x0C, 0x0C, 0x0C};

    for (int i = 0; i < city_->data_length(); ++i) {
      auto type = city_->type(i);
      int x_pos = (i % MAP_TILES_PER_ROW) * 2;
      int y_pos = (i / MAP_TILES_PER_ROW) * 2;
      ViewRect pixel_location(x_pos, y_pos, 2, 2);
      uint8_t palette_index =
          palette_map[((type & 0x0F) << 4) | ((type & 0xF0) >> 4)];
      far_map.fill_rect(pixel_location, palette_index);
    }
    far_map_cache_ = std::move(far_map);
  }

  ctx.surface->blit_raw(*far_map_cache_,
                        {TABBED_VIEW_OFFSET_X, TABBED_VIEW_OFFSET_Y});
}

void CityView::render_map_data(const RenderContext& ctx) {
  const auto z = ZoomConfig::for_zoom_level(zoom_);
  switch (zoom_) {
  case ZoomLevel::CLOSE:
    render_map_zoom_close(ctx);
    break;
  case ZoomLevel::MEDIUM:
    render_map_zoom_medium(ctx);
    break;
  case ZoomLevel::FAR:
    render_map_zoom_far(ctx);
    break;
  }
}

WorldPoint CityView::world_pos_of_index(int index) const {
  const int tx = City::index_to_x(index);
  const int ty = City::index_to_y(index);
  return WorldPoint{tx * BASE_MAP_TILE_WIDTH, ty * BASE_MAP_TILE_HEIGHT};
}

gfx::Surface CityView::create_zoom_box_cursor(int width, int height,
                                              int border_width) {
  assert(width > 0 && height > 0 && border_width > 0);

  gfx::Surface cursor{width, height, gfx::PaletteHandle{0}};

  const uint8_t yellow_idx = 16;

  cursor.fill_rect({0, 0, width, height}, gfx::TRANSPARENT);

  gfx::draw::box(cursor, {0, 0}, {width - 2, height - 2}, yellow_idx,
                 border_width);

  return cursor;
}

bool CityView::is_valid_zoom_box_location(ViewPoint screen_p) {
  const auto z = ZoomConfig::for_zoom_level(zoom_);

  // Convert screen coordinates to map-relative coordinates
  const int map_x = screen_p.x;
  const int map_y = screen_p.y;

  // valid region is the map area minus the box size (so the box fits entirely)
  const int max_x = TABBED_VIEWPORT_WIDTH - z.zoom_box_w;
  const int max_y = TABBED_VIEWPORT_HEIGHT - z.zoom_box_h;

  return (map_x >= 0) && (map_y >= 0) && (map_x <= max_x) && (map_y <= max_y);
}

void CityView::render_with_color(
    Surface& map, std::function<uint32_t(core::DistrictId)> color_fn) {
  // District cells are in world coordinates (5120x3840 world space)
  const Size ws = camera_.world_size();
  const int cell_width_world = ws.w / NUMBER_OF_CELL_COLUMNS; // 5120/20 = 256
  const int cell_height_world = ws.h / NUMBER_OF_CELL_ROWS;   // 3840/15 = 256

  const PxOffset tab_offset{0, 0};

  // Iterate over every district cell
  for (int i = 0; i < NUMBER_OF_CELL_ROWS * NUMBER_OF_CELL_COLUMNS; ++i) {
    // Decide color for this cell
    auto district_id = city_->district_by_district_index(i);
    uint8_t color = color_fn(district_id);

    // Figure out row/col in the district grid
    int row = i / NUMBER_OF_CELL_COLUMNS;
    int col = i % NUMBER_OF_CELL_COLUMNS;

    // World position of this district cell (top-left corner)
    const WorldPoint cell_world_pos{col * cell_width_world,
                                    row * cell_height_world};

    const ViewPoint view_pos = camera_.world_to_view(cell_world_pos);

    const ViewPoint screen_pos = view_pos + tab_offset;

    // Cell size in view space (scaled by camera zoom)
    const int cell_width_view = cell_width_world / camera_.zoom();
    const int cell_height_view = cell_height_world / camera_.zoom();

    // Create the cell rectangle in screen space
    ViewRect cell_rect{screen_pos.x, screen_pos.y, cell_width_view,
                       cell_height_view};

    // Viewport bounds - local surface coordinates (0,0) to (width, height)
    const auto view_sz = camera_.view_size();
    const ViewRect viewport{0, 0, view_sz.w, view_sz.h};

    // Calculate intersection (clipped region)
    const auto clipped_opt = intersect_rect(viewport, cell_rect);
    if (!clipped_opt) {
      continue; // Outside viewport, skip
    }

    // Draw the clipped cell with pattern
    // Pass world origin to anchor pattern to world space
    draw_cell_with_pattern(map, *clipped_opt, color, cell_world_pos,
                           screen_pos);
  }
}

void CityView::render_districts(const RenderContext& ctx) {
  Surface district_map(TABBED_VIEWPORT_WIDTH, TABBED_VIEWPORT_HEIGHT,
                       PaletteHandle{DISTRICT_COLORS_INDEX});
  // Initialize the bez_id to Color map only once
  if (!color_map_initialized_) {
    for (const auto& district : city_->districts()) {
      core::DistrictId id{district->id()};
      uint8_t color = district->color();
      bezid_to_color_map_[id] = color;
    }
    color_map_initialized_ = true;
  }

  // We'll define a small lambda that, given a district_id, returns
  // a stable color from district_color_list.
  auto color_fn = [&](core::DistrictId district_id) -> uint8_t {
    auto it = bezid_to_color_map_.find(district_id);
    if (it != bezid_to_color_map_.end()) {
      return it->second;
    }
    Logger::debug("Unknown color for {}", district_id.value);
    return 0;
  };

  // Now we just call our new helper
  render_with_color(district_map, color_fn);
  ctx.surface->blit_raw(district_map,
                        {TABBED_VIEW_OFFSET_X, TABBED_VIEW_OFFSET_Y});
}

// Player colors for the control map overlay (palette 0 indices).
// These map player index 0-7 to visually distinct palette colors.
constexpr std::array<uint8_t, NUMBER_OF_PLAYERS> PLAYER_CONTROL_COLORS = {
    15, // Player 0: White
    9,  // Player 1: Black
    27, // Player 2: Dark blue
    5,  // Player 3: Brown
    22, // Player 4: Orange
    20, // Player 5: Red
    24, // Player 6: Light blue
    17, // Player 7: Yellow
};

void CityView::render_with_cell_color(Surface& map,
                                      std::function<uint8_t(int)> color_fn) {
  const Size ws = camera_.world_size();
  const int cell_width_world = ws.w / NUMBER_OF_CELL_COLUMNS;
  const int cell_height_world = ws.h / NUMBER_OF_CELL_ROWS;

  for (int i = 0; i < NUMBER_OF_CELL_ROWS * NUMBER_OF_CELL_COLUMNS; ++i) {
    uint8_t color = color_fn(i);

    int row = i / NUMBER_OF_CELL_COLUMNS;
    int col = i % NUMBER_OF_CELL_COLUMNS;

    const WorldPoint cell_world_pos{col * cell_width_world,
                                    row * cell_height_world};
    const ViewPoint view_pos = camera_.world_to_view(cell_world_pos);
    const ViewPoint screen_pos = view_pos;

    const int cell_width_view = cell_width_world / camera_.zoom();
    const int cell_height_view = cell_height_world / camera_.zoom();

    ViewRect cell_rect{screen_pos.x, screen_pos.y, cell_width_view,
                       cell_height_view};

    const auto view_sz = camera_.view_size();
    const ViewRect viewport{0, 0, view_sz.w, view_sz.h};

    const auto clipped_opt = intersect_rect(viewport, cell_rect);
    if (!clipped_opt) {
      continue;
    }

    draw_cell_with_pattern(map, *clipped_opt, color, cell_world_pos,
                           screen_pos);
  }
}

void CityView::render_control(const RenderContext& ctx) {
  Surface control_surface(TABBED_VIEWPORT_WIDTH, TABBED_VIEWPORT_HEIGHT,
                          PaletteHandle{DISTRICT_COLORS_INDEX});

  constexpr uint8_t light_brown = 1;
  constexpr uint8_t dark_brown = 3;

  auto color_fn = [&](int cell_index) -> uint8_t {
    auto owner = city_->control_map().cell_owner(cell_index);
    if (owner.has_value()) {
      return PLAYER_CONTROL_COLORS[owner->value];
    }
    // Fallback: district-based neutral color
    auto district_id = city_->district_by_district_index(cell_index);
    return city_->district(district_id).control_screen_alternate_color()
               ? dark_brown
               : light_brown;
  };

  render_with_cell_color(control_surface, color_fn);
  ctx.surface->blit_raw(control_surface,
                        {TABBED_VIEW_OFFSET_X, TABBED_VIEW_OFFSET_Y});
}

void CityView::update_tab_positions() {
  static const std::map<CityViewTabType, std::array<ViewPoint, 3>>
      tab_positions = {
          {CityViewTabType::MAP,
           {ViewPoint{15, 46}, ViewPoint{28, 57}, ViewPoint{42, 70}}},
          {CityViewTabType::DISTRICTS,
           {ViewPoint{54, 45}, ViewPoint{68, 58}, ViewPoint{82, 69}}},
          {CityViewTabType::CONTROL,
           {ViewPoint{94, 44}, ViewPoint{108, 58}, ViewPoint{122, 70}}}};

  for (std::size_t i = 0; i < tabs_.size(); ++i) {
    CityViewTab& tab = tabs_[i];
    auto pos_it = tab_positions.find(tab.type());
    if (pos_it != tab_positions.end()) {
      tab.position(pos_it->second[i]);
    }
  }

  tabs_cache_.reset();
  mark_dirty();
}

void CityView::set_active_tab(CityViewTabType type) {
  auto it =
      std::find_if(tabs_.begin(), tabs_.end(),
                   [type](const CityViewTab& t) { return t.type() == type; });
  if (it != tabs_.end()) {
    CityViewTab tab = *it;
    tabs_.erase(it);
    tabs_.push_back(tab);
    active_tab_type_ = type;
    update_tab_positions();
  }
}

void CityView::render_tabs(const RenderContext& ctx) {
  if (!tabs_cache_) {
    Surface tabs({ctx.surface->width(), ctx.surface->height()}, palette_);
    tabs.blit_raw(context_.vga->get("gfx/back21"));

    const std::array<uint8_t, 3> colors = {246, 249, 251};

    int index = 0;
    for (const CityViewTab& tab : tabs_) {
      const ViewRect r = tab.rect();
      const uint8_t color_idx = colors[index++];

      std::string tab_label;
      switch (tab.type()) {
      case CityViewTabType::MAP:
        tab_label = "MAP";
        break;
      case CityViewTabType::DISTRICTS:
        tab_label = "DISTRICTS";
        break;
      case CityViewTabType::CONTROL:
        tab_label = "CONTROL";
        break;
      }
      const int text_color = 240;

      tabs.fill_rect(r, color_idx);
      context_.font->render_text_onto(tabs, tab_label,
                                      r.point() + PxOffset{12, 6},
                                      FontFace::CSTANDAR, text_color);
    }
    tabs_cache_ = std::move(tabs);
  }

  ctx.surface->blit_raw(*tabs_cache_);

  // register interactions with the tabs:
  for (const CityViewTab& tab : tabs_) {
    const ViewRect r = tab.rect();
    interactions_.create_frame_item(
        r, compat::format("citytab_{}", magic_enum::enum_name(tab.type())),
        [this, ttype = tab.type()](views::ui::InteractionEvent ev) {
          switch (ev.type) {
          case views::ui::InteractionEvent::Type::ButtonDown:
            if (ev.button == views::ui::MouseButton::Left) {
              if (ttype != active_tab_type_) {
                set_active_tab(ttype);
              }
              mark_dirty();
            }
            break;
          default:
            break;
          }
        });
  }
}

void CityView::draw_cell_with_pattern(Surface& map, ViewRect cell,
                                      uint8_t cell_color,
                                      WorldPoint cell_world_origin,
                                      ViewPoint cell_screen_origin) {
  int max_x = TABBED_VIEWPORT_WIDTH;
  int max_y = TABBED_VIEWPORT_HEIGHT;

  // Clip the rectangle to viewport bounds
  if (cell.x + cell.w > max_x) {
    cell.w = max_x - cell.x;
  }
  if (cell.y + cell.h > max_y) {
    cell.h = max_y - cell.y;
  }

  // Fill the cell with the base color
  map.fill_rect(cell, cell_color);

  // Line pattern constants
  const int line_gap = 6;  // Number of normal pixels before black
  const int line_size = 2; // Size of the black "dot"
  const uint8_t line_color = 240;

  const int zoom = camera_.zoom();

  // Calculate the pattern offset based on world coordinates
  // At zoom=4 (MEDIUM), world coordinates are 4x larger than view coordinates
  // We divide by zoom to get the pattern coordinate in view space
  const int pattern_offset_x =
      (cell_world_origin.x / zoom) % (line_gap + line_size);
  const int pattern_offset_y =
      (cell_world_origin.y / zoom) % (line_gap + line_size);

  // Loop through each pixel in the cell
  for (int y = 0; y < cell.h; ++y) {
    for (int x = 0; x < cell.w; ++x) {
      // Calculate position relative to cell origin
      int cell_rel_x = (cell.x - cell_screen_origin.x) + x;
      int cell_rel_y = (cell.y - cell_screen_origin.y) + y;

      // Apply pattern offset from world position
      int pattern_x = cell_rel_x + pattern_offset_x;
      int pattern_y = cell_rel_y + pattern_offset_y;

      int wrapped_y = pattern_y % (line_gap + line_size);

      if ((pattern_y % 2 == 0) && ((pattern_x % 8 == line_gap - wrapped_y))) {
        // Draw a 2x2 pixel "dot" at this position
        for (int dy = 0; dy < line_size; ++dy) {
          for (int dx = 0; dx < line_size; ++dx) {
            if (cell.x + x + dx < max_x && cell.y + y + dy < max_y) {
              map.put({cell.x + x + dx, cell.y + y + dy}, line_color);
            }
          }
        }
      }
    }
  }
}

void CityView::create_buttons() {
  // Most buttons cost time to click:
  auto side_menu_entry = [this](auto open) {
    return [this, open] {
      state_.side_menu_charge_pending(true);
      open();
    };
  };

  input_panel_->add_button(
      Icon::AIRPLANE, ButtonType::Square, ButtonPosition::TopLeft, "Map",
      side_menu_entry([this] {
        ui_push_store_position<EuroMapState>(
            CityViewReentryMode::RESTORE_PARTIAL, MapMode::TRAVEL);
      }));

  input_panel_->add_button(
      Icon::CHARACTER, ButtonType::Square, ButtonPosition::TopCenter,
      "Character", side_menu_entry([this] {
        state_.ui().push<PlayerProfileState>(state_.active_player()->id());
      }));

  input_panel_->add_button(
      Icon::DISK, ButtonType::Square, ButtonPosition::TopRight, "Disk options",
      side_menu_entry(
          [this] { state_.ui().push<ScreenTag>(ScreenTag::Kind::DiskMenu); }));

  input_panel_->add_button(
      Icon::MEGAPHONE, ButtonType::Square, ButtonPosition::MiddleLeft,
      "Advertising", side_menu_entry([this] {
        state_.ui().push<ScreenTag>(ScreenTag::Kind::Advertising);
      }));

  input_panel_->add_button(
      Icon::QUESTION_MARK, ButtonType::Square, ButtonPosition::MiddleCenter,
      "Market research", side_menu_entry([this] {
        state_.ui().push<MarketResearchState>(
            state_.active_district_id().value_or(core::DistrictId{0}));
      }));

  input_panel_->add_button(
      Icon::NEWSPAPER, ButtonType::Square, ButtonPosition::MiddleRight,
      "Newspaper", side_menu_entry([this] {
        state_.ui().push<ScreenTag>(ScreenTag::Kind::Newspaper);
      }));

  input_panel_->add_button(
      Icon::GRAPH, ButtonType::Square, ButtonPosition::BottomLeft,
      "Global stats", side_menu_entry([this] {
        state_.ui().push<ScreenTag>(ScreenTag::Kind::GlobalStats);
      }));

  input_panel_->add_button(Icon::COMPUTER, ButtonType::Square,
                           ButtonPosition::BottomCenter, "Terminal",
                           side_menu_entry([this] {
                             state_.ui().push<TerminalState>(std::nullopt);
                           }));

  input_panel_->add_button(Icon::SUNSET, ButtonType::Square,
                           ButtonPosition::BottomRight, "End turn",
                           [this] { state_.turn().request_end_of_turn(); });
}

void CityView::create_dropdowns() {
  constexpr int dd_btn_width = 94;
  constexpr int dd_btn_height = 32;
  constexpr int dd_btn_offset_x = 0;
  constexpr int dd_btn_offset_y = 7;
  constexpr int dd_btn_spacing_x = 96;

  constexpr Dropdown DROPDOWN_FIRST = Dropdown::PLAYER;
  constexpr Dropdown DROPDOWN_LAST = Dropdown::PROPERTIES;

  // Where each dropdown list will open
  static constexpr std::array<ViewPoint, 4> origins{
      ViewPoint{8, 12},   // PLAYER
      ViewPoint{56, 12},  // RANGE
      ViewPoint{128, 12}, // LOCATIONS
      ViewPoint{184, 12}, // PROPERTIES
  };

  // Create the four buttons + empty lists
  for (int i = static_cast<int>(DROPDOWN_FIRST);
       i <= static_cast<int>(DROPDOWN_LAST); ++i) {
    Dropdown dd = static_cast<Dropdown>(i);
    ViewRect location{dd_btn_offset_x + (dd_btn_spacing_x * i), dd_btn_offset_y,
                      dd_btn_width, dd_btn_height};
    dropdowns_.add_dropdown(dd, location, origins[i]);
  }

  // Players and their ranges
  for (auto* player : state_.players()) {
    const auto pid = player->id();
    const std::string& pname = player->name();

    dropdowns_.add_item(Dropdown::PLAYER, pname, [this, pid] {
      highlight_mode(CityViewHighlight{
          .mode = HighlightMode::PLAYER,
          .data = pid,
      });
    });
    dropdowns_.add_item(Dropdown::RANGE, pname, [this, pid] {
      highlight_mode(CityViewHighlight{
          .mode = HighlightMode::PLAYER_RANGE,
          .data = pid,
      });
    });
  }

  // Building / location filters
  auto add_location_item = [this](BuildingType type, const char* label) {
    dropdowns_.add_item(Dropdown::LOCATIONS, label, [this, type] {
      highlight_mode(CityViewHighlight{
          .mode = HighlightMode::BUSINESS_PROPERTIES,
          .data = type,
      });
    });
  };

  using enum BuildingType;
  add_location_item(CHURCH, "CHURCH");
  add_location_item(BANK, "BANK");
  add_location_item(CINEMA, "CINEMA");
  add_location_item(SCHOOL, "SCHOOL");
  add_location_item(DISCO, "DISCO");
  add_location_item(BAR, "BAR");
  add_location_item(DEPARTMENT_STORE, "DEPARTMENT STORE");
  add_location_item(HOTEL, "HOTEL");
  add_location_item(RESTAURANT, "RESTAURANT");
  add_location_item(DEPOT, "DEPOT");
  add_location_item(BUSINESS_PREMISES, "BUSINESS PREMISES");

  // Property status filters
  auto add_property_item = [this](PremiseStatus status, const char* label) {
    dropdowns_.add_item(Dropdown::PROPERTIES, label, [this, status] {
      highlight_mode(CityViewHighlight{
          .mode = HighlightMode::PROPERTY_STATUS,
          .data = status,
      });
    });
  };
  using enum PremiseStatus;
  add_property_item(NOT_AVAILABLE, "NOT AVAILABLE");
  add_property_item(FOR_SALE, "FOR SALE");
  add_property_item(FOR_RENT, "FOR RENT");
  add_property_item(FOR_SALE_RENT, "FOR SALE/RENT");
}

void CityView::clear_highlights() {
  highlight_enabled_ = false;
  for (int i = 0; i < city_->data_length(); ++i) {
    highlighted_cells_[i] = HighlightType::NONE;
  }
}

void CityView::render_highlights(const RenderContext& ctx) {
  if (!highlight_enabled_) {
    return;
  }

  const PxOffset tab_offset{TABBED_VIEW_OFFSET_X, TABBED_VIEW_OFFSET_Y};

  for (std::size_t i = 0; i < highlighted_cells_.size(); ++i) {
    if (highlighted_cells_[i] == HighlightType::DOT) {
      // Get world position of this tile (always in CLOSE scale: 32-pixel tiles)
      const int tx = City::index_to_x(i);
      const int ty = City::index_to_y(i);
      const WorldPoint world_pos{
          tx * BASE_MAP_TILE_WIDTH * BASE_MAP_ZOOM_MULTIPLIER,
          ty * BASE_MAP_TILE_HEIGHT * BASE_MAP_ZOOM_MULTIPLIER};

      // Convert to view coordinates
      const ViewPoint view_pos = camera_.world_to_view(world_pos);

      // Add tab offset for MEDIUM/FAR zoom (at CLOSE, offset is 0)
      const ViewPoint screen_pos = view_pos + tab_offset;

      // Cell size in view space (scaled by camera zoom)
      const int world_box_size = BASE_MAP_TILE_WIDTH * BASE_MAP_ZOOM_MULTIPLIER;
      const int cell_view_size = world_box_size / camera_.zoom();

      // At zoom FAR a cell-sized dot is too small to see, so it's always
      // doubled there; qol_improvements doubles it again on top of that
      int view_box_size = cell_view_size;
      if (zoom_ == ZoomLevel::FAR) {
        view_box_size *= 2;
        if (context_.config->options().qol_improvements) {
          view_box_size *= 2;
        }
      }

      // Center the (possibly enlarged) box on the cell
      const int center_offset = (cell_view_size - view_box_size) / 2;
      const ViewRect dest_rect{screen_pos.x + center_offset,
                               screen_pos.y + center_offset, view_box_size,
                               view_box_size};

      // Clip to viewport
      const auto view_sz = camera_.view_size();
      const ViewRect viewport{TABBED_VIEW_OFFSET_X, TABBED_VIEW_OFFSET_Y,
                              view_sz.w, view_sz.h};

      const auto clipped_opt = intersect_rect(viewport, dest_rect);
      if (!clipped_opt) {
        continue; // Outside viewport
      }

      const uint8_t yellow_idx = 16;
      ctx.surface->fill_rect(*clipped_opt, yellow_idx);
    }
    else if (highlighted_cells_[i] == HighlightType::RANGE) {
      const int tx = City::index_to_x(i);
      const int ty = City::index_to_y(i);
      const WorldPoint world_pos{
          tx * BASE_MAP_TILE_WIDTH * BASE_MAP_ZOOM_MULTIPLIER,
          ty * BASE_MAP_TILE_HEIGHT * BASE_MAP_ZOOM_MULTIPLIER};

      const ViewPoint view_pos = camera_.world_to_view(world_pos);
      const ViewPoint screen_pos = view_pos + tab_offset;

      // Original: 2x2 at far zoom, 4x4 at medium zoom (doubled for 2x scale)
      const int view_box_size = zoom_ == ZoomLevel::MEDIUM ? 8 : 4;

      const ViewRect dest_rect{screen_pos.x, screen_pos.y, view_box_size,
                               view_box_size};

      const auto view_sz = camera_.view_size();
      const ViewRect viewport{TABBED_VIEW_OFFSET_X, TABBED_VIEW_OFFSET_Y,
                              view_sz.w, view_sz.h};

      const auto clipped_opt = intersect_rect(viewport, dest_rect);
      if (!clipped_opt) {
        continue;
      }

      const uint8_t range_color_idx = 15;
      ctx.surface->fill_rect(*clipped_opt, range_color_idx);
    }
  }
}

void CityView::highlight_if(
    std::function<HighlightType(int)>
        predicate) { // NOLINT(performance-unnecessary-value-param)
  highlight_enabled_ = true;
  for (int i = 0; i < city_->data_length(); ++i) {
    auto highlight = predicate(i);
    highlighted_cells_[i] = highlight;
  }
}

void CityView::highlight_properties(PremiseStatus status) {
  highlight_if([this, status](int idx) {
    auto bp = city_->property_by_index(idx);
    if (bp == nullptr) {
      return HighlightType::NONE;
    }
    return bp->status() == status ? HighlightType::DOT : HighlightType::NONE;
  });
}

void CityView::highlight_player(core::PlayerId player_id, bool range) {
  if (!range) {
    highlight_if([this, player_id](int idx) {
      auto* property = city_->property_by_index(idx);
      if (!property) {
        return HighlightType::NONE;
      }
      auto owner = property->owner_or_tenant();
      if (owner && *owner == player_id) {
        return HighlightType::DOT;
      }
      return HighlightType::NONE;
    });
    return;
  }

  // Range mode: mark all tiles within Manhattan distance of each owned
  // restaurant, forming filled diamond shapes like the original game.
  highlight_enabled_ = true;
  std::fill(highlighted_cells_.begin(), highlighted_cells_.end(),
            HighlightType::NONE);

  for (int i = 0; i < city_->data_length(); ++i) {
    auto* property = city_->property_by_index(i);
    if (!property) {
      continue;
    }
    auto owner = property->owner_or_tenant();
    if (!owner || *owner != player_id || !property->has_restaurant()) {
      continue;
    }
    auto* restaurant = state_.restaurant(*property->restaurant_id());
    if (!restaurant) {
      continue;
    }
    const int cx = City::index_to_x(i);
    const int cy = City::index_to_y(i);
    const int r = restaurant->range();

    for (int dy = -r; dy <= r; ++dy) {
      int max_dx = r - std::abs(dy);
      for (int dx = -max_dx; dx <= max_dx; ++dx) {
        int tx = cx + dx;
        int ty = cy + dy;
        if (tx < 0 || tx >= MAP_TILES_PER_ROW || ty < 0 ||
            ty >= MAP_TILES_PER_COLUMN) {
          continue;
        }
        int idx = City::x_y_to_index(tx, ty);
        highlighted_cells_[idx] = HighlightType::RANGE;
      }
    }
  }
}

void CityView::highlight_business_premises() {
  highlight_if([this](int idx) {
    return city_->is_business_premise_by_index(idx) ? HighlightType::DOT
                                                    : HighlightType::NONE;
  });
}

void CityView::highlight_building_type(BuildingType type) {
  const auto btype_raw = static_cast<uint8_t>(type);
  highlight_if([this, btype_raw](int idx) {
    auto cell_type = (uint8_t)city_->building_type_by_index(idx);
    return (cell_type == btype_raw) ? HighlightType::DOT : HighlightType::NONE;
  });
}

core::DistrictId CityView::district_at(WorldPoint pw) {
  // pw is always in normalized world coordinates because camera always uses
  // 5120x3840 as world_size
  const Size ws = camera_.world_size();

  // Derive district-cell size in zoom level CLOSE pixels
  const int cell_w = ws.w / NUMBER_OF_CELL_COLUMNS;
  const int cell_h = ws.h / NUMBER_OF_CELL_ROWS;

  // Which col/row in the 20x15 district map?
  const int col = std::clamp(pw.x / cell_w, 0, NUMBER_OF_CELL_COLUMNS - 1);
  const int row = std::clamp(pw.y / cell_h, 0, NUMBER_OF_CELL_ROWS - 1);

  const int district_index = (row * NUMBER_OF_CELL_COLUMNS) + col;
  // Logger::debug("district_index {}, from row {} and NOCLL {} + col {}",
  // district_index, row, NUMBER_OF_CELL_COLUMNS, col);
  return city_->district_by_district_index(district_index);
}

void CityView::store_state_in_city(CityViewReentryMode entry_mode) {
  state_.city_view_view_state(capture_view_state(entry_mode));
  should_save_map_state_on_exit_ = false;
}

void CityView::update_water_palette(const RenderContext& ctx) {
  // Start from the season palette (which already has indices 28-31 overridden)
  // and apply water cycling on top
  auto pal = season_palette_;
  const auto& cycle = water_cycles_[water_animation_.frame_index];
  for (size_t i = 0; i < water_ovr_.dst_indices.size(); ++i) {
    pal[water_ovr_.dst_indices[i]] = season_palette_[cycle[i]];
  }
  ctx.surface->colors(pal);
}

void CityView::setup_water_animation() {
  static constexpr std::array<uint8_t, 4> WATER_INDICES = {140, 141, 142, 143};

  static constexpr std::array<std::array<uint8_t, 4>, 4> WATER_CYCLES = {{
      {140, 141, 142, 143},
      {141, 142, 143, 140},
      {142, 143, 140, 141},
      {143, 140, 141, 142},
  }};

  water_indices_.assign(WATER_INDICES.begin(), WATER_INDICES.end());

  water_cycles_.resize(WATER_CYCLES.size());
  for (size_t i = 0; i < WATER_CYCLES.size(); i++) {
    water_cycles_[i] = std::vector<uint8_t>(std::begin(WATER_CYCLES[i]),
                                            std::end(WATER_CYCLES[i]));
  }

  water_animation_.ticks_per_frame = 10;

  water_buf_ = gfx::PaletteBank::instance().colors(palette_);
  water_ovr_.dst_indices = water_indices_;
  water_ovr_.src_palette_id = palette_.id;

  // Start animation if we're already at close zoom
  if (zoom_ == ZoomLevel::CLOSE) {
    water_animation_.request_start();
  }
}

void CityView::build_property_status_recipe(int clicked_index) {
  state_.status().clear_overlay();
  if (city_->property_by_index(clicked_index)) {
    if (property_info_toggle_ == clicked_index) {
      // Second click on the same property: show customer demographics
      status_controller_.show_city_customer_info(
          {.state = &state_, .city = city_}, clicked_index);
      property_info_toggle_ = std::nullopt;
    }
    else {
      // First click (or different property): show property info and arm toggle
      status_controller_.show_city_property_info(
          {.state = &state_, .city = city_}, clicked_index);
      property_info_toggle_ = clicked_index;
    }
  }
  else if (city_->is_named_building_by_index(clicked_index)) {
    status_controller_.show_city_named_building(
        {.state = &state_, .city = city_}, clicked_index);
  }
  else {
    status_controller_.show_city_type_name({.state = &state_, .city = city_},
                                           clicked_index);
  }
}

void CityView::build_default_city_status_recipe() {
  status_controller_.show_city_default({.state = &state_, .city = city_});
}

void CityView::highlight_mode(const CityViewHighlight& highlight) {
  highlight_mode_ = highlight;
  clear_highlights();
  highlight_enabled_ = true;

  switch (highlight.mode) {
  case HighlightMode::PLAYER:
    highlight_player(std::get<core::PlayerId>(highlight.data), false);
    break;
  case HighlightMode::PLAYER_RANGE:
    highlight_player(std::get<core::PlayerId>(highlight.data), true);
    break;
  case HighlightMode::BUSINESS_PROPERTIES: {
    const auto btype = std::get<BuildingType>(highlight.data);
    switch (btype) {
    case BuildingType::BUSINESS_PREMISES:
      highlight_business_premises();
      break;
    default:
      highlight_building_type(btype);
      break;
    }
    break;
  }
  case HighlightMode::PROPERTY_STATUS:
    highlight_properties(std::get<PremiseStatus>(highlight.data));
    break;
  default:
    break;
  }
}

CityViewMapState CityView::capture_view_state(CityViewReentryMode mode) const {
  CityViewMapState state;
  if (mode == CityViewReentryMode::RESTORE_ALL) {
    state.scroll = camera_.scroll();
    state.zoom = zoom_;
    state.active_tab = active_tab_type_;

    Logger::debug("CityView::capture_view_state - scroll: {}, zoom: {}",
                  state.scroll, static_cast<int>(state.zoom));
  }
  state.highlight = highlight_mode_;

  assert(highlight_mode_.mode >= HighlightMode::NONE &&
         highlight_mode_.mode <= HighlightMode::PROPERTY_STATUS);
  assert(std::holds_alternative<std::monostate>(highlight_mode_.data) ||
         std::holds_alternative<BuildingType>(highlight_mode_.data) ||
         std::holds_alternative<core::PlayerId>(highlight_mode_.data) ||
         std::holds_alternative<PremiseStatus>(highlight_mode_.data));

  return state;
}

void CityView::restore_view_state(const CityViewMapState& state) {
  zoom_ = state.zoom;
  set_active_tab(state.active_tab);
  apply_zoom_profile(zoom_);

  WorldPoint scroll_pos = (state.scroll.x < 0 || state.scroll.y < 0)
                              ? WorldPoint{0, 0}
                              : state.scroll;
  camera_.scroll(scroll_pos);
  camctrl_.reset();

  highlight_mode(state.highlight);
}

int CityView::index_at(WorldPoint p) const {
  // Convert world pixel position -> tile coordinates.
  // World space is always at CLOSE scale
  constexpr int tw = BASE_MAP_TILE_WIDTH * BASE_MAP_ZOOM_MULTIPLIER;
  constexpr int th = BASE_MAP_TILE_HEIGHT * BASE_MAP_ZOOM_MULTIPLIER;

  const int tx = p.x / tw;
  const int ty = p.y / th;

  // Clamp to valid map bounds to avoid overflow.
  const int clamped_tx = std::clamp(tx, 0, MAP_TILES_PER_ROW - 1);
  const int clamped_ty = std::clamp(ty, 0, MAP_TILES_PER_COLUMN - 1);

  return City::x_y_to_index(clamped_tx, clamped_ty);
}
