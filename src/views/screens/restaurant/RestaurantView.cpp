/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/restaurant/RestaurantView.h"

#include <algorithm>
#include <cmath>

#include "core/Demographics.h"
#include "core/city/District.h"
#include "core/city/Property.h"
#include "core/events/LeventId.h"
#include "core/finance/Ledger.h"
#include "core/restaurant/Furniture.h"
#include "core/restaurant/RestaurantTypes.h"
#include "core/state/GameState.h"
#include "core/status/StatusHub.h"
#include "core/util/LegacyConstants.h"
#include "engine/Fatal.h"
#include "engine/FormatCompat.h"
#include "engine/Point.h"
#include "engine/Random.h"
#include "engine/audio/AudioManager.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/ICursorManager.h"
#include "engine/ui/InteractionEvent.h"
#include "gfx/Blit.h"
#include "views/base/BobRenderer.h"
#include "views/base/ViewDefs.h"
#include "views/screens/restaurant/RestaurantStats.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/AlertPopup.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/ui/popups/GuestTalkPopup.h"
#include "views/ui/popups/JukeboxPopup.h"
#include "views/ui/popups/TvPopup.h"

using namespace views::base;
using namespace LegacyConstants;

RestaurantView::RestaurantView(GameContext& context, GameState& state,
                               RestaurantViewState r_state)
    : MainPanelView(context, state), k0_(context_.vga->get("gfx/k0")),
      furniture_renderer_(BobRenderer("bob/moebel.bob", context_.bob)),
      oven_renderer_(BobRenderer("bob/kuebobs.bob", context_.bob)),
      pizza_renderer_(BobRenderer("bob/moebel.bob", context_.bob)),
      pizza_bob_(context_.bob->get("bob/moebel.bob")),
      restaurant_(state_.restaurant(r_state.id)),
      background_{Size{
          1, 1}}, // Minimal size; will be properly sized in prepare_background
      view_mode_(state_.restaurant_placement().empty()
                     ? r_state.mode
                     : RestaurantViewMode::PLAYER_NEW_FURNITURE),
      camctrl_{context.input, camera_},
      furniture_placement_(state_.restaurant_placement()),
      return_to_supplier_type_(r_state.return_to_supplier_type),
      status_controller_(state.status()),
      listener_{context_.input->register_listener(*this)} {
  assert(restaurant_);

  // Set this restaurant as the active one so other views know which restaurant
  // we're working with
  state_.active_restaurant(r_state.id);
  state_.prevent_end_of_day(false);
  if (furniture_placement_.empty()) {
    state_.resume_time_advancement();
  }
  else {
    state_.pause_time_advancement(); // only advance explicitly per item placed
  }

  // Check if we're placing kitchenware (ovens) - if so, start in kitchen view
  // Also check if we're placing regular furniture while the restaurant is open,
  // in which case we need to close it temporarily.
  bool post_waiting_customers_msg = false;
  if (!furniture_placement_.empty()) {
    for (const auto& batch : furniture_placement_) {
      if (batch.furniture &&
          batch.furniture->category() == FurnitureCategory::KITCHENWARE) {
        kitchen_view_ = true;
        break;
      }
    }
    if (!kitchen_view_) {
      restaurant_->wants_open(false);
      restaurant_->add_reputation_damage(-16, 16);
      post_waiting_customers_msg = true;
    }
  }

  if (r_state.reopen_restaurant) {
    restaurant_->wants_open(true);
  }

  set_up_buttons();

  scroll_edges_.add_interactions(interactions_);

  // Calculate background dimensions based on restaurant/kitchen size
  calculate_background_dimensions();
  register_interactions_viewport();
  if (view_mode_ == RestaurantViewMode::PLAYER_NEW_FURNITURE) {
    init_active_batch_if_needed();
  }

  // Update buttons based on whether we have furniture to place
  update_buttons_for_furniture_state();

  // Set up camera with proper world size
  camera_.view_size(Size{MAIN_PANEL_WIDTH, MAIN_PANEL_HEIGHT});
  camera_.world_size(Size{background_width_, background_height_});
  camera_.align(Camera::Align::Start, Camera::Align::End);

  // Initialize scroll to show the bottom of the restaurant (where entrance is)
  // Alignment handles small restaurants; scroll handles large ones
  const int init_x = 0;
  const int init_y = std::max(0, background_height_ - MAIN_PANEL_HEIGHT);
  camera_.scroll(WorldPoint{init_x, init_y});

  camctrl_.tile_grid(RESTAURANT_TILE_WIDTH, RESTAURANT_TILE_HEIGHT);
  camctrl_.reset();

  if (view_mode_ == RestaurantViewMode::OPPONENT_VIEW) {
    status_controller_.show_competitor_restaurant(
        {.state = &state_, .city = state_.active_city()}, *restaurant_);
  }
  else {
    status_controller_.show_restaurant(
        {.state = &state_, .rest = restaurant_, .label = "RESTAURANT"});
  }

  if (post_waiting_customers_msg) {
    state_.status().post(StatusMsg{
        .payload = "The restaurant is closed while the furniture is being "
                   "positioned. There are some customers waiting impatiently "
                   "outside!",
        .scope = StatusScope::Restaurant,
        .type = TransientType::Ephemeral,
        .ttl = std::nullopt,
        .coalesce_key = "furniture_reposition_close",
    });
  }

  if (const auto music = restaurant_->jukebox_music(); music > 0) {
    if (context_.audio) {
      context_.audio->play_music(static_cast<audio::MusicId>(music - 1));
    }
  }

  update_restaurant_noise();
}

RestaurantView::~RestaurantView() {
  state_.prevent_end_of_day(true);
  if (context_.audio) {
    context_.audio->stop_music();
    if (current_restaurant_noise_) {
      context_.audio->stop_sound(*current_restaurant_noise_);
    }
  }
}

void RestaurantView::update_restaurant_noise() {
  if (!context_.audio || !context_.config->options().restaurant_fx) {
    return;
  }

  // Stop any currently playing restaurant noise
  if (current_restaurant_noise_) {
    context_.audio->stop_sound(*current_restaurant_noise_);
    current_restaurant_noise_.reset();
  }

  // No noise when closed, in kitchen view, or when jukebox is playing
  if (kitchen_view_ || !restaurant_->is_open(state_.time().time_of_day()) ||
      restaurant_->jukebox_music() > 0) {
    return;
  }

  const auto [guests, seats] = restaurant_->guest_occupancy();
  if (guests <= 0 || seats <= 0) {
    return;
  }

  const float occupancy =
      static_cast<float>(guests) / static_cast<float>(seats);

  audio::SoundId noise;
  if (occupancy > 0.81f) {
    noise = audio::SoundId::RESTAURANT_NOISE_3;
  }
  else if (occupancy > 0.51f) {
    noise = audio::SoundId::RESTAURANT_NOISE_2;
  }
  else if (occupancy > 0.21f) {
    noise = audio::SoundId::RESTAURANT_NOISE_1;
  }
  else {
    // No sound if not enough people
    return;
  }

  current_restaurant_noise_ = noise;
  context_.audio->play_sound(noise, true);
}

bool RestaurantView::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }

  const auto current_id = restaurant_->id();
  const auto owner_pid =
      restaurant_->property()
          ->owner_or_tenant(); // or however you obtain the player
  if (!owner_pid) {
    fatal("My current restaurant is not owned by me?!");
  }

  auto push_view = [&](core::RestaurantId rid) {
    // Update the stored city view scroll to center on the new restaurant
    auto* rest = state_.restaurant(rid);
    if (rest && rest->property()) {
      auto cvs = state_.city_view_view_state();
      cvs.center_on_index(rest->property()->index());
      state_.city_view_view_state(cvs);
    }

    state_.ui().push<RestaurantViewState>(RestaurantViewState{
        .id = rid,
        .mode = view_mode_,
    });
  };

  switch (ev.action) {
  case input::Action::NextRestaurant: {
    if (auto next = state_.next_restaurant(*owner_pid, current_id)) {
      if (*next != current_id) {
        push_view(*next);
      }
    }
    return true;
  }
  case input::Action::PreviousRestaurant: {
    if (auto prev = state_.prev_restaurant(*owner_pid, current_id)) {
      if (*prev != current_id) {
        push_view(*prev);
      }
    }
    return true;
  }
  case input::Action::ToggleRestaurantView: {
    toggle_kitchen_view();
    mark_dirty();
    return true;
  }
  default: {
    break;
  }
  }

  return false;
}

void RestaurantView::set_up_buttons() {
  if (view_mode_ == RestaurantViewMode::OPPONENT_VIEW) {
    input_panel_->add_button(Icon::CHEFS_HAT, ButtonType::Horizontal,
                             ButtonPosition::TopLeft, "Challenge",
                             []() { /* noop */ });

    input_panel_->add_button(Icon::BOMB, ButtonType::Horizontal,
                             ButtonPosition::MiddleLeft, "Sabotage", [this]() {
                               view_mode_ = RestaurantViewMode::SABOTAGE;
                               mark_dirty();
                             });

    input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                             ButtonPosition::BottomRight, "Back",
                             [this]() { state_.ui().pop(); });

    return;
  }
  if (view_mode_ == RestaurantViewMode::SABOTAGE) {
    input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                             ButtonPosition::BottomRight, "Back",
                             [this]() { state_.ui().pop(); });
    return;
  }

  input_panel_->add_button(
      Icon::KEY, ButtonType::Square, ButtonPosition::TopLeft, "Open/close",
      [this]() {
        context_.popup->origin(ViewPoint{52, 80});
        if (restaurant_->wants_open()) {
          context_.popup->create<ConfirmPopup>(
              "Close restaurant?", [this](bool close) {
                Logger::debug("Closing restaurant: {}", close ? "Yes" : "No");
                if (close) {
                  restaurant_->wants_open(false);
                  restaurant_->add_reputation_damage(-16, 16);
                  update_restaurant_noise();
                }
                mark_dirty();
              });
        }
        else {
          if (restaurant_->is_force_closed()) {
            context_.popup->create<AlertPopup>(
                "You can not open the restaurant|right now!", []() {});
            return;
          }

          const auto oven_count = restaurant_->count_ovens();
          if (oven_count <= 0) {
            context_.popup->create<AlertPopup>(
                "You still need ovens|before you can open the|restaurant!",
                []() {});
            return;
          }

          const auto cook_count = restaurant_->count_cooks();
          if (cook_count == 0) {
            context_.popup->create<AlertPopup>(
                "You still need kitchen|staff before you can open|the "
                "restaurant!",
                []() {});
            return;
          }

          const auto waiter_count = restaurant_->count_waiters();
          if (waiter_count == 0) {
            context_.popup->create<AlertPopup>(
                "You still need waiters|before you can open the|restaurant!",
                []() {});
            return;
          }

          const auto menu_count = restaurant_->count_menu_items();
          if (menu_count == 0) {
            context_.popup->create<AlertPopup>(
                "You still need some pizzas|on the menu before you|can open "
                "the restaurant!",
                []() {});
            return;
          }

          context_.popup->create<ConfirmPopup>(
              "Open restaurant?", [this](bool open) {
                Logger::debug("Opening restaurant: {}", open ? "Yes" : "No");
                if (open) {
                  restaurant_->wants_open(true);
                  update_restaurant_noise();
                  if (auto* player = state_.active_player()) {
                    state_.check_restaurant_milestones(*player);
                  }
                }

                mark_dirty();
              });
        }
      });

  input_panel_->add_button(Icon::STOCK, ButtonType::Square,
                           ButtonPosition::TopCenter, "Stock",
                           [this]() { state_.ui().push<StockMainState>(); });

  input_panel_->add_button(Icon::MENU, ButtonType::Square,
                           ButtonPosition::TopRight, "Menu", [this]() {
                             assert(restaurant_);
                             state_.ui().push<RestaurantMenuState>(
                                 restaurant_->id());
                           });

  input_panel_->add_button(
      Icon::FURNISHING, ButtonType::Square, ButtonPosition::MiddleLeft,
      "Furnishing",
      [this]() { state_.ui().push<PurchasingState>(SupplierType::FURNITURE); });

  input_panel_->add_button(
      Icon::CHARACTER2, ButtonType::Square, ButtonPosition::MiddleCenter,
      "Staff",
      [this]() { state_.ui().push_screen(ScreenTag::Kind::StaffView); });

  input_panel_->add_button(
      Icon::PARCHMENT, ButtonType::Square, ButtonPosition::MiddleRight,
      "Recipe list", [this]() {
        state_.ui().push<PizzaManagerState>(PizzaManagerMode::Recipes);
      });

  input_panel_->add_button(
      Icon::GRAPH, ButtonType::Square, ButtonPosition::BottomLeft,
      "Restaurant stats",
      [this]() { state_.ui().push<RestaurantStatsState>(restaurant_->id()); });

  input_panel_->add_button(Icon::COMPUTER, ButtonType::Square,
                           ButtonPosition::BottomCenter, "Terminal",
                           [this]() { state_.ui().push<TerminalState>(); });

  input_panel_->add_button(
      Icon::ARROW_UP_DOWN, ButtonType::Square, ButtonPosition::BottomRight,
      "Animated stats",
      [this]() { state_.ui().push<AnimatedStatsState>(restaurant_->id()); });
}

void RestaurantView::register_interactions_viewport() {
  const ViewRect vp_local{WOOD_BORDER_WIDTH_LEFT, WOOD_BORDER_HEIGHT_TOP,
                          MAIN_PANEL_WIDTH, MAIN_PANEL_HEIGHT};

  interactions_.create_persistent_item(
      vp_local, "rest_view_catchall", [this](views::ui::InteractionEvent ev) {
        if (ev.type != views::ui::InteractionEvent::Type::ButtonDown) {
          return;
        }

        if (ev.button == views::ui::MouseButton::Right) {
          if (on_cursor_) {
            on_cursor_->rotate();
            // Only update cursor if it's visible (mouse is in viewport)
            if (show_furniture_cursor_) {
              context_.cursor->custom_cursor(
                  current_bob_renderer().image(*on_cursor_), false);
            }
            mark_dirty();
          }
          else {
            zoom_out_of_restaurant_view();
          }
          return;
        }
        // Left click handling
        if (on_cursor_ && ghost_furniture_topleft_) {
          // When holding furniture, always try to place at ghost position
          // This takes precedence over other interactions (door, etc.)
          handle_mouse_click(*ghost_furniture_topleft_);
        }
        else {
          // Normal click - use actual mouse position
          const WorldPoint p = camera_.view_to_world(ev.local);
          handle_mouse_click(p);
        }
        mark_dirty();
      });

  // Background interaction for furniture ghost tracking
  // This layer doesn't block other interactions (door, furniture, etc.)
  interactions_.create_persistent_item(
      vp_local, "furniture_ghost_tracker",
      [this](views::ui::InteractionEvent ev) {
        if (!on_cursor_) {
          return; // Only process if holding furniture
        }

        switch (ev.type) {
        case views::ui::InteractionEvent::Type::Enter:
          // Don't automatically show cursor on enter - let the next Hover event
          // determine whether to show cursor or ghost based on clamping
          break;

        case views::ui::InteractionEvent::Type::Hover: {
          // Update ghost position as mouse moves
          const WorldPoint mouse_world = camera_.view_to_world(ev.local);

          // Always update ghost position (clamped to interior bounds - handles
          // sliding)
          ghost_furniture_topleft_ = clamp_furniture_to_interior(mouse_world);

          // Check if furniture at clamped position would be fully visible
          // If mouse != ghost position, we've clamped, so hide cursor and show
          // ghost
          const bool clamped = (ghost_furniture_topleft_->x != mouse_world.x ||
                                ghost_furniture_topleft_->y != mouse_world.y);

          // Check if furniture at mouse position would intersect walls
          const WorldRect candidate = candidate_rect(mouse_world, *on_cursor_);
          const bool over_wall = intersects_wall(candidate);

          if (over_wall || clamped) {
            // Mouse over wall or furniture clamped - restore regular cursor
            if (show_furniture_cursor_) {
              context_.cursor->clear_custom_cursor();
              show_furniture_cursor_ = false;
            }
          }
          else {
            // Mouse over valid area and not clamped - show custom cursor
            if (!show_furniture_cursor_) {
              context_.cursor->custom_cursor(
                  current_bob_renderer().image(*on_cursor_), false);
              show_furniture_cursor_ = true;
            }
          }
          mark_dirty();
          break;
        }

        case views::ui::InteractionEvent::Type::Leave:
          // Disable custom cursor when leaving viewport
          if (show_furniture_cursor_) {
            context_.cursor->clear_custom_cursor();
            show_furniture_cursor_ = false;
          }
          mark_dirty();
          break;

        default:
          break;
        }
      },
      {}, // no tooltip
      views::ui::InteractionLayer::Background);
}

void RestaurantView::handle_mouse_click(WorldPoint map_point) {
  if (on_cursor_) {
    WorldRect cand{};
    const PlacedFurniture* nearest_table = nullptr;
    WorldRect nearest_table_rect{};

    if (!can_place_now(map_point, cand, nearest_table, nearest_table_rect)) {
      return;
    }

    face_chair_towards_nearest_table_if_any(cand, nearest_table,
                                            nearest_table_rect);

    // Link chair to nearest table before placing
    if (nearest_table && on_cursor_ &&
        on_cursor_->furniture()->category() == FurnitureCategory::CHAIR) {
      on_cursor_->linked_furniture_id(nearest_table->id());
    }

    // cand already has the correct top-left position (calculated by
    // candidate_rect) Pass the top-left corner for placement
    place_cursor_item(cand.point());
  }
  else {
    update_status_display(nullptr);
  }
}

void RestaurantView::toggle_kitchen_view() {
  kitchen_view_ = !kitchen_view_;

  // Recalculate background size for the new view
  calculate_background_dimensions();

  // Update camera with new world size
  camera_.world_size(Size{background_width_, background_height_});

  // Set alignment based on view:
  // - Restaurant view: bottom-left (Start, End) to show entrance at bottom
  // - Kitchen view: top-right (End, Start) to show dividing wall on right
  if (kitchen_view_) {
    camera_.align(Camera::Align::End, Camera::Align::Start);
    // Scroll to show right side of kitchen (where the door is)
    const int init_x = std::max(0, background_width_ - MAIN_PANEL_WIDTH);
    camera_.scroll(WorldPoint{init_x, 0});
  }
  else {
    camera_.align(Camera::Align::Start, Camera::Align::End);
    // Start at bottom of restaurant (where entrance is)
    const int init_y = std::max(0, background_height_ - MAIN_PANEL_HEIGHT);
    camera_.scroll(WorldPoint{0, init_y});
  }

  camctrl_.reset();
  mark_dirty();
}

void RestaurantView::calculate_background_dimensions() {

  // Exact size of restaurant/kitchen including walls (+2 for walls on each
  // side)
  auto room_size =
      kitchen_view_ ? restaurant_->kitchen_size() : restaurant_->size();
  background_width_ = (room_size.w + 2) * RESTAURANT_TILE_WIDTH;
  background_height_ = (room_size.h + 2) * RESTAURANT_TILE_HEIGHT;
}

void RestaurantView::tick() {
  if (camctrl_.tick()) {
    mark_dirty();
  }

  // Tick guest animations
  if (restaurant_->is_open(state_.time().time_of_day()) && !kitchen_view_) {
    for (auto& [chair_id, controller] : guest_controllers_) {
      controller.tick();
    }
    mark_dirty();
  }
}

void RestaurantView::handle_furniture_click(const PlacedFurniture* pf) {
  const auto prop_id = pf->id();
  if (!restaurant_->is_open(state_.time().time_of_day()) &&
      last_clicked_furniture_index_ &&
      *last_clicked_furniture_index_ == prop_id) {
    auto picked = restaurant_->layout()->remove_by_id(prop_id, kitchen_view_);
    assert(picked && "remove_by_id failed");
    assert(picked->id() == prop_id && "picked id changed unexpectedly");

    // Create a batch with count=1 for this picked-up furniture
    FurnitureBatch batch{.furniture = picked->furniture(),
                         .count = 1,
                         .bob_state = picked->bob_state()};
    furniture_placement_.push_back(batch);

    // Set as active batch (this will create on_cursor_ and set up the cursor)
    set_active_batch(furniture_placement_.size() - 1);

    update_buttons_for_furniture_state();

    last_clicked_furniture_index_.reset();
    mark_dirty();
  }
  else if (restaurant_->is_open(state_.time().time_of_day()) &&
           last_clicked_furniture_index_ &&
           *last_clicked_furniture_index_ == prop_id) {
    const auto ftype = pf->furniture()->type();
    if (ftype == FurnitureType::JUKEBOX) {
      context_.popup->origin({66, 72});
      context_.popup->create<JukeboxPopup>(*restaurant_);
    }
    else if (ftype == FurnitureType::TV_AERIAL ||
             ftype == FurnitureType::TV_CABLE ||
             ftype == FurnitureType::TV_SATELLITE) {
      context_.popup->origin({66, 72});
      context_.popup->create<TvPopup>(*restaurant_);
    }
    else {
      StatusMsg msg{
          .payload =
              "You can rearrange furniture only when the restaurant is closed!",
          .scope = StatusScope::Restaurant,
          .type = TransientType::Ephemeral,
          .ttl = std::nullopt,
          .coalesce_key = "furniture_reposition_close",
      };
      state_.status().post(msg);
    }
  }
  else if (!on_cursor_) {
    last_clicked_furniture_index_ = prop_id;
    update_status_display(pf);
  }
}

void RestaurantView::prepare_background() {
  if (background_.width() != background_width_ ||
      background_.height() != background_height_) {
    background_ = Surface(background_width_, background_height_);
  }
  else {
    background_.fill(0);
  }
}

std::optional<ViewRect>
RestaurantView::map_rect_to_viewport(const WorldRect& world_rect,
                                     const Camera& camera) {
  const WorldRect vis = camera.visible_world_rect();
  if (!intersect_rect(world_rect, vis)) {
    return std::nullopt;
  }

  const WorldRect clipped = intersect(world_rect, vis);
  ViewRect view_rect = camera.world_to_view(clipped);

  return view_rect;
}

void RestaurantView::draw_floor_tiles(ViewRect interior) {
  const int tile_w = RESTAURANT_TILE_WIDTH;
  const int tile_h = RESTAURANT_TILE_HEIGHT;

  assert(interior.x % tile_w == 0 && interior.y % tile_h == 0);
  assert(interior.w % tile_w == 0 && interior.h % tile_h == 0);

  const int cols = interior.w / tile_w;
  const int rows = interior.h / tile_h;
  if (cols <= 0 || rows <= 0) {
    return;
  }

  // Sources
  const ViewRect base_floor_src{FLOOR_TILE_OFFSET * tile_w, 0, tile_w, tile_h};

  // Pattern sheet for restaurant floor (kitchen uses base tile only)
  const auto pattern = restaurant_->layout()->floor_pattern();
  const std::string k_filename =
      compat::format("gfx/k{}", pattern.vendor_id + 1);
  const Surface& pattern_sheet = context_.vga->get(k_filename);

  auto blit_tile = [this](const Surface& src, const ViewRect& src_rect, int x,
                          int y) {
    background_.blit(src, src_rect, ViewPoint{x, y});
  };

  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const int dst_x = interior.x + (c * tile_w);
      const int dst_y = interior.y + (r * tile_h);

      if (kitchen_view_) {
        // Kitchen: single base tile, no pattern/shadow variant
        blit_tile(k0_, base_floor_src, dst_x, dst_y);
      }
      else {
        // Restaurant: 2x2 pattern + "shadow" variant (top 2 rows, rightmost 2
        // cols)
        const bool even_row = (r & 1) == 0;
        const bool even_col = (c & 1) == 0;
        // Find the right tile for the pattern (01\n23)
        const std::size_t idx =
            even_row ? (even_col ? 0 : 1) : (even_col ? 2 : 3);

        const uint8_t tile_index = pattern.tile_indices[idx];
        const bool in_shadow = (r < 2) || (c >= cols - 2);
        const int src_y_off = in_shadow ? tile_h : 0;

        const ViewRect src{static_cast<int>(tile_index) * tile_w, src_y_off,
                           tile_w, tile_h};
        blit_tile(pattern_sheet, src, dst_x, dst_y);
      }
    }
  }
}

void RestaurantView::render_scene_passes() {
  furniture_draw_counter_ = 0;

  // Seats first
  render_placement_layer(PlacementLayer::Seating);

  // Then the guest legs
  render_guest_legs();

  // Then the tables (can be glass/transparent)
  render_placement_layer(PlacementLayer::Tables);

  // Guest upper body/arms
  render_guest_upper();

  // Pizzas on tables
  render_guest_pizzas();

  // Rest of the furniture and stuff
  render_placement_layer(PlacementLayer::Top);
}

inline WorldRect rect_of(const PlacedFurniture& pf, const BobRenderer& r) {
  const Size sz = r.size(pf);
  return WorldRect{pf.origin().x, pf.origin().y, sz.w, sz.h};
}

inline ViewRect rect_centered_at(Size sz, ViewPoint c) {
  return ViewRect{c.x - (sz.w / 2), c.y - (sz.h / 2), sz.w, sz.h};
}

// Only block against other Top-layer DECOR that sits on (intersects) this
// table.
bool RestaurantView::decor_conflicts_on_table(
    const WorldRect& candidate_map, const WorldRect& table_map_rect) const {
  const auto& r = current_bob_renderer();
  for (const auto& up : current_furniture_list()) {
    const auto* of = up->furniture();
    if (of->category() != FurnitureCategory::DECOR ||
        of->placement_layer() != PlacementLayer::Top) {
      continue;
    }
    const WorldRect other_map = rect_of(*up, r);
    if (!other_map.intersects(table_map_rect)) {
      continue; // decor not on this table
    }
    if (candidate_map.intersects(other_map)) {
      return true; // conflicts with decor already on this table
    }
  }
  return false;
}

void RestaurantView::render_placement_layer(PlacementLayer placement) {
  const auto& r = current_bob_renderer();
  const auto& pf_list = current_furniture_list();
  const Point wall_offsets =
      Point{RESTAURANT_TILE_WIDTH, RESTAURANT_TILE_HEIGHT};

  // Panel (view) rect for clipping
  const Size vsz = camera_.view_size();
  const ViewRect panel{0, 0, vsz.w, vsz.h};

  for (const auto& pf_uptr : pf_list) {
    const PlacedFurniture* pf = pf_uptr.get();
    if (pf->furniture()->placement_layer() != placement) {
      continue;
    }

    WorldPoint wp = pf->origin();
    const Size sz = r.size(*pf);

    const WorldRect furn_world{wp.x, wp.y, sz.w, sz.h};
    if (!camera_.possibly_visible(furn_world)) {
      continue;
    }

    // Map to view space and clip to the panel
    const ViewRect furniture_box = camera_.world_to_view(furn_world);

    // Only process items whose view-space bounding boxes overlap the visible
    // part of this panel
    if (auto clipped = intersect_rect(furniture_box, panel)) {
      const ViewRect hit = *clipped;
      const auto fid = pf->id();
      const auto category = pf->furniture()->category();
      const std::string token_id =
          compat::format("furn_{}_{}", kitchen_view_ ? 'K' : 'R', fid.value);

      interactions_.create_frame_item(
          hit, token_id,
          [this, fid, hit, furn_world](views::ui::InteractionEvent ev) {
            if (ev.type != views::ui::InteractionEvent::Type::Click) {
              return;
            }
            if (ev.button == views::ui::MouseButton::Left) {
              auto* pf = restaurant_->layout()->find_by_id(fid, kitchen_view_);
              if (!pf) {
                return;
              }
              if (on_cursor_ && ghost_furniture_topleft_) {
                const auto* curf = on_cursor_->furniture();

                // Special case: placing decor on tables requires clicking
                // inside the table
                if (curf->category() == FurnitureCategory::DECOR &&
                    curf->placement_layer() == PlacementLayer::Top &&
                    pf->furniture()->category() == FurnitureCategory::TABLE) {

                  // item-local -> panel-local (view space)
                  ViewPoint p_view{hit.x + ev.local.x, hit.y + ev.local.y};

                  // Table rect in world space
                  const WorldRect table_world = furn_world;

                  // Center-only: require click within inner area
                  constexpr int inner_margin_px = 6;
                  const WorldRect inner_world =
                      table_world.inset(inner_margin_px, inner_margin_px);

                  // panel-local (view) -> world space
                  const WorldPoint click_world = camera_.view_to_world(p_view);

                  if (!inner_world.contains(click_world)) {
                    handle_furniture_click(pf);
                    return;
                  }

                  // Use the ghost position (already tracking the correct
                  // placement position)
                  const auto& r = current_bob_renderer();
                  const Size decor_sz = r.size(*on_cursor_);
                  const WorldRect decor_world{ghost_furniture_topleft_->x,
                                              ghost_furniture_topleft_->y,
                                              decor_sz.w, decor_sz.h};

                  // Block only on other Top-layer decor on this same table
                  if (decor_conflicts_on_table(decor_world, table_world)) {
                    return;
                  }

                  // Place decor at the ghost position
                  place_cursor_item(*ghost_furniture_topleft_);
                  mark_dirty();
                  return;
                }

                // General case: placing any other furniture (chairs, tables,
                // etc.)
                handle_mouse_click(*ghost_furniture_topleft_);
                return;
              }

              // If there is a guest seated at this chair, clicking selects the
              // guest rather than picking up the furniture.
              if (auto idx = click_guest(fid)) {
                return;
              }

              // Default: furniture selection / pickup (when not holding
              // furniture)
              handle_furniture_click(pf);
            }
            else if (ev.button == views::ui::MouseButton::Right) {
              if (on_cursor_) {
                on_cursor_->rotate();
                // Only update cursor if it's visible (mouse is in viewport)
                if (show_furniture_cursor_) {
                  context_.cursor->custom_cursor(
                      current_bob_renderer().image(*on_cursor_), false);
                }
                mark_dirty();
              }
              else {
                zoom_out_of_restaurant_view();
              }
            }
          });
    }

    // Render furniture to background_ in world coordinates
    const auto& bob_surface = r.image(*pf);

    // wp is the furniture origin (top-left corner) in world coordinates
    // Blit directly at this position
    ViewPoint dst{wp.x, wp.y};
    background_.blit_masked(bob_surface, dst);
    ++furniture_draw_counter_;
  }
}

void RestaurantView::render_guest_legs() {
  const auto guests = visible_guests();
  if (guests.empty()) {
    return;
  }

  // Draw onto background_
  RenderContext guest_ctx{.surface = &background_};

  for (const auto& g : guests) {
    // Render lower body - behind tables
    ViewPoint anchor_view{g.torso_anchor_map.x, g.torso_anchor_map.y};
    guest_renderer_.render_lower_body(guest_ctx, anchor_view, g.pose);
  }
}

void RestaurantView::render_guest_upper() {
  const auto guests = visible_guests();
  if (guests.empty()) {
    return;
  }

  RenderContext guest_ctx{.surface = &background_}; // adapt as needed

  for (const auto& g : guests) {
    // Render upper body - on top of tables
    ViewPoint anchor_view{g.torso_anchor_map.x, g.torso_anchor_map.y};
    guest_renderer_.render_upper_body(guest_ctx, anchor_view, g.pose);
  }
}

void RestaurantView::render_guest_pizzas() {
  const auto guests = visible_guests();
  if (guests.empty()) {
    return;
  }

  RenderContext guest_ctx{.surface = &background_};

  // Pizza offsets from x_offsets_pizza_maybe/y_offsets_pizza_maybe at 2x
  constexpr int8_t pizza_offset_x[4] = {4, -24, 2, 10};
  constexpr int8_t pizza_offset_y[4] = {6, 0, -28, 0};

  for (const auto& g : guests) {
    // Only render pizza if guest is eating
    if (!g.pose.eating) {
      continue;
    }

    int direction = static_cast<int>(g.pose.facing) & 3;
    ViewPoint pizza_pos{g.torso_anchor_map.x + pizza_offset_x[direction],
                        g.torso_anchor_map.y + pizza_offset_y[direction]};

    // Render pizza sprite (frame 0x154 from moebel.bob)
    Surface pizza_sprite = pizza_bob_.frame(0x154);
    guest_ctx.surface->blit_masked(pizza_sprite, pizza_pos);
  }
}

std::span<const GuestRenderItem> RestaurantView::visible_guests() const {
  guests_cache_.clear();

  // Only show guests in restaurant view (not kitchen) and when open
  if (kitchen_view_ || !restaurant_->is_open(state_.time().time_of_day())) {
    shuffled_chairs_.clear();
    return guests_cache_;
  }

  const auto [guests_to_show, seat_count] = restaurant_->guest_occupancy();
  if (guests_to_show <= 0) {
    return guests_cache_;
  }

  // Build shuffled chair list once per open period
  if (shuffled_chairs_.empty()) {
    for (const auto& pf : restaurant_->layout()->placed_furniture()) {
      if (!pf || pf->furniture()->category() != FurnitureCategory::CHAIR) {
        continue;
      }
      if (!pf->linked_furniture_id().has_value()) {
        continue;
      }
      shuffled_chairs_.push_back(pf.get());
    }
    engine::random::shuffle(shuffled_chairs_.begin(), shuffled_chairs_.end());
  }

  if (shuffled_chairs_.empty()) {
    return guests_cache_;
  }

  // Compute demographic fractions for guest appearance distribution
  constexpr int DEMO_COUNT = static_cast<int>(ConsumerType::Count);
  const auto& customer_counts = restaurant_->economy().customer_counts();

  int total_daily = 0;
  for (auto ct : ALL_CONSUMER_TYPES) {
    total_daily += customer_counts[ct];
  }

  std::array<float, DEMO_COUNT> demo_fractions{};
  if (total_daily > 0) {
    for (int i = 0; i < DEMO_COUNT; ++i) {
      demo_fractions[i] =
          static_cast<float>(customer_counts[static_cast<ConsumerType>(i)]) /
          total_daily;
    }
  }

  // Distribute guests across demographics (largest-remainder)

  std::array<int, DEMO_COUNT> demo_guests{};
  {
    int assigned = 0;
    for (int i = 0; i < DEMO_COUNT; ++i) {
      demo_guests[i] = static_cast<int>(demo_fractions[i] * guests_to_show);
      assigned += demo_guests[i];
    }
    int remainder = guests_to_show - assigned;
    for (int i = 0; i < DEMO_COUNT && remainder > 0; ++i) {
      if (demo_fractions[i] > 0.0f) {
        demo_guests[i]++;
        --remainder;
      }
    }
  }

  // Assign guests to chairs grouped by demographic (but since the chairs are
  // shuffled this distributes demographics randomly across the restaurant)

  using namespace views::screens::restaurant;

  constexpr int8_t seating_offset_x[4] = {-4, -3, -3, 4};
  constexpr int8_t seating_offset_y[4] = {2, -5, -6, -9};

  int chair_idx = 0;
  for (int demo_i = 0; demo_i < DEMO_COUNT && chair_idx < guests_to_show;
       ++demo_i) {
    for (int g = 0; g < demo_guests[demo_i] && chair_idx < guests_to_show;
         ++g, ++chair_idx) {
      const PlacedFurniture* pf = shuffled_chairs_[chair_idx];
      const core::FurnitureId chair_id = pf->id();

      auto it = guest_controllers_.find(chair_id);
      if (it == guest_controllers_.end()) {
        CharacterController controller;
        controller.set_customer_type(static_cast<ConsumerType>(demo_i));
        controller.set_eating((chair_id.value % 2) == 0);
        it = guest_controllers_.emplace(chair_id, std::move(controller)).first;
      }
      else {
        it->second.set_customer_type(static_cast<ConsumerType>(demo_i));
      }

      const int rotation = pf->bob_state() % 4;
      Facing guest_facing;
      switch (rotation) {
      case 0:
        guest_facing = Facing::Up;
        break;
      case 1:
        guest_facing = Facing::Right;
        break;
      case 2:
        guest_facing = Facing::Down;
        break;
      case 3:
        guest_facing = Facing::Left;
        break;
      default:
        guest_facing = Facing::Up;
      }

      const WorldPoint chair_pos = pf->origin();
      const Size furniture_size = furniture_renderer_.size(*pf);
      const WorldPoint guest_pos{chair_pos.x + (furniture_size.w / 2) +
                                     (seating_offset_x[rotation] * 2),
                                 chair_pos.y + (furniture_size.h / 2) +
                                     (seating_offset_y[rotation] * 2)};

      CharacterPose pose = it->second.pose();
      pose.facing = guest_facing;
      const int face_index = (demo_i * 4) + (chair_id.value % 4);
      guests_cache_.push_back(GuestRenderItem{
          .torso_anchor_map = guest_pos,
          .pose = pose,
          .chair_id = chair_id,
          .interacted_before = false,
          .face_index = face_index,
      });
    }
  }

  return guests_cache_;
}

std::optional<int> RestaurantView::click_guest(core::FurnitureId chair_id) {
  for (int i = 0; i < static_cast<int>(guests_cache_.size()); ++i) {
    if (guests_cache_[i].chair_id == chair_id) {
      const auto& g = guests_cache_[i];
      const bool talked = guest_talked_to_.contains(chair_id);
      GuestInfo info{
          .demographic = g.pose.customer_type,
          .face_index = g.face_index,
          .been_talked_to = talked,
          .has_pizza = g.pose.eating,
      };

      // Pick a random pizza from the menu and fill in pizza feedback data
      const auto& entries = restaurant_->menu().entries();
      if (!entries.empty()) {
        const auto& entry = entries[engine::random::rand_max_n(
            static_cast<int>(entries.size()))];
        const auto* recipe =
            state_.active_player()->recipes().find(entry.recipe_id);
        if (recipe) {
          const auto& pizza = recipe->pizza();
          const auto demo = g.pose.customer_type;
          info.pizza_name = std::string(recipe->display_name());
          info.pizza_taste = pizza.popularity()[demo];
          info.pizza_price = entry.price;
          info.pizza_quality = pizza.normality();
          if (const auto* prop = restaurant_->property()) {
            info.district_price_level =
                prop->district().average_income_factor();
          }
        }
      }

      context_.popup->origin({62, 60});
      context_.popup->create<GuestTalkPopup>(info, *restaurant_);
      guest_talked_to_[chair_id] = true;
      return i;
    }
  }
  return std::nullopt;
}

const BobRenderer& RestaurantView::current_bob_renderer() const {
  return kitchen_view_ ? oven_renderer_ : furniture_renderer_;
}

const std::vector<std::unique_ptr<PlacedFurniture>>&
RestaurantView::current_furniture_list() const {
  return kitchen_view_ ? restaurant_->layout()->placed_kitchen_furniture()
                       : restaurant_->layout()->placed_furniture();
}

bool RestaurantView::intersects_wall(const WorldRect& r) const {
  const auto room_size =
      kitchen_view_ ? restaurant_->kitchen_size() : restaurant_->size();

  // Walls are 1 tile thick on all sides
  constexpr int tile_w = LegacyConstants::RESTAURANT_TILE_WIDTH;
  constexpr int tile_h = LegacyConstants::RESTAURANT_TILE_HEIGHT;

  // Interior rectangle in world coordinates
  const WorldRect interior{
      tile_w,               // x: after left wall
      tile_h,               // y: after top wall
      room_size.w * tile_w, // width: interior tiles only
      room_size.h * tile_h  // height: interior tiles only
  };

  return !interior.contains(r);
}

void RestaurantView::prune_empty_batches() {
  std::erase_if(furniture_placement_, [](const FurnitureBatch& b) {
    return b.count <= 0 || b.furniture == nullptr;
  });
}

void RestaurantView::init_active_batch_if_needed() {
  prune_empty_batches();
  if (furniture_placement_.empty()) {
    return;
  }
  set_active_batch(active_batch_index_ ? *active_batch_index_ : 0);
}

FurnitureBatch* RestaurantView::current_batch() {
  if (!active_batch_index_) {
    return nullptr;
  }
  if (*active_batch_index_ >= furniture_placement_.size()) {
    return nullptr;
  }
  return &furniture_placement_[*active_batch_index_];
}

void RestaurantView::set_active_batch(size_t idx) {
  if (idx >= furniture_placement_.size()) {
    return;
  }
  active_batch_index_ = idx;

  // Refresh the cursor preview from the active batch
  if (auto* b = current_batch()) {
    on_cursor_ = make_cursor_for_batch(*b);
    // We don't set custom_cursor here because the mouse might be outside
    // the viewport when furniture is picked up. The furniture_ghost_tracker
    // interaction handler will set it on Enter/Hover events when appropriate.
  }
}

void RestaurantView::advance_to_next_batch() {
  if (furniture_placement_.empty()) {
    active_batch_index_.reset();
    on_cursor_.reset();
    context_.cursor->clear_custom_cursor();
    return;
  }
  size_t start = active_batch_index_.value_or(0);
  size_t i = start;
  const size_t n = furniture_placement_.size();

  for (size_t step = 0; step < n; ++step) {
    i = (start + 1 + step) % n;
    if (furniture_placement_[i].count > 0) {
      set_active_batch(i);
      return;
    }
  }

  // None left with count>0
  active_batch_index_.reset();
  on_cursor_.reset();
  context_.cursor->clear_custom_cursor();
}

void RestaurantView::decrement_or_erase_current_and_maybe_advance(
    bool auto_advance_batches) {
  if (auto* b = current_batch()) {
    if (b->count > 0) {
      --b->count;
    }
    if (b->count == 0) {
      // Erase the empty batch
      const size_t idx = *active_batch_index_;
      furniture_placement_.erase(furniture_placement_.begin() + idx);

      if (furniture_placement_.empty() || !auto_advance_batches) {
        // Clear cursor if no batches remain OR if auto-advance is disabled
        // (auto-advance disabled prevents picking up furniture that was just
        // moved elsewhere in the current restaurant)
        active_batch_index_.reset();
        on_cursor_.reset();
        context_.cursor->clear_custom_cursor();

        check_placement_complete_and_navigate_back();
      }
      else {
        // Auto-advance: after erase, idx now points at the next item
        set_active_batch(std::min(idx, furniture_placement_.size() - 1));
      }
      return;
    }

    // Still items left in this batch: refresh cursor
    set_active_batch(*active_batch_index_);
  }
}

void RestaurantView::throw_away_one_from_current_batch() {
  if (!current_batch()) {
    return;
  }
  decrement_or_erase_current_and_maybe_advance();
  mark_dirty();
}

std::unique_ptr<PlacedFurniture>
RestaurantView::make_cursor_for_batch(const FurnitureBatch& b) {
  if (!b.furniture) {
    return nullptr;
  }

  // An off-map dummy origin; will be re-positioned at click time.
  // Use stored rotation if available (from picked-up furniture)
  const int bob_state = b.bob_state.value_or(0);

  auto pf = std::make_unique<PlacedFurniture>(
      b.furniture, restaurant_->layout()->next_furniture_id(), WorldPoint{0, 0},
      core::FurnitureId{UNLINKED_FURNITURE}, bob_state);
  return pf;
}

WorldRect RestaurantView::candidate_rect(WorldPoint p,
                                         const PlacedFurniture& ghost) const {
  const auto size = current_bob_renderer().size(ghost);
  // Furniture top-left should be at the click point
  return WorldRect{p.x, p.y, size.w, size.h};
}

bool RestaurantView::intersects_any_blocking_furniture(
    const WorldRect& candidate, const PlacedFurniture& cand,
    const PlacedFurniture*& nearest_table_out,
    WorldRect& nearest_table_rect_out) const {

  nearest_table_out = nullptr;
  nearest_table_rect_out = {};

  const auto* cand_f = cand.furniture();
  const auto cand_cat = cand_f->category();
  const auto cand_layer = cand_f->placement_layer();

  const auto dist_sq = [](WorldPoint a, WorldPoint b) {
    const int dx = a.x - b.x;
    const int dy = a.y - b.y;
    return (dx * dx) + (dy * dy);
  };

  for (const auto& pf : current_furniture_list()) {
    const auto other_size = current_bob_renderer().size(*pf);
    const WorldRect other{pf->origin(), other_size};

    if (!candidate.intersects(other)) {
      continue;
    }

    const auto* other_f = pf->furniture();
    const auto other_cat = other_f->category();
    const auto other_layer = other_f->placement_layer();

    // Allowed overlaps

    // CHAIR on TABLE:
    if (cand_cat == FurnitureCategory::CHAIR &&
        other_cat == FurnitureCategory::TABLE) {
      // Disallow if chair center lies inside the table
      if (other.contains(candidate.center())) {
        return true; // reject
      }

      // Track nearest table center
      if (!nearest_table_out) {
        nearest_table_out = pf.get();
        nearest_table_rect_out = other;
      }
      else {
        const WorldPoint best_c = nearest_table_rect_out.center();
        const WorldPoint this_c = other.center();
        const WorldPoint cc = candidate.center();
        if (dist_sq(this_c, cc) < dist_sq(best_c, cc)) {
          nearest_table_out = pf.get();
          nearest_table_rect_out = other;
        }
      }
      continue; // allowed overlap; keep checking others
    }

    // TABLE on CHAIR
    if (cand_cat == FurnitureCategory::TABLE &&
        other_cat == FurnitureCategory::CHAIR) {
      // Disallow if table center lies inside the chair
      if (other.contains(candidate.center())) {
        return true; // reject
      }
      continue; // allowed overlap; keep checking others
    }

    // DECOR (Top layer) on TABLE:
    if (cand_cat == FurnitureCategory::DECOR &&
        cand_layer == PlacementLayer::Top &&
        other_cat == FurnitureCategory::TABLE) {
      continue; // allowed overlap; keep checking others
    }

    // Default: any other overlap blocks placement
    return true;
  }

  // no blocking collisions
  return false;
}

Rotation RestaurantView::rotation_towards(const WorldRect& from,
                                          const WorldRect& to) {
  const WorldPoint fc = from.center();
  const WorldPoint tc = to.center();
  const WorldDelta wd = tc - fc;

  if (std::abs(wd.dx) >= std::abs(wd.dy)) {
    return (wd.dx >= 0) ? Rotation::EAST : Rotation::WEST;
  }
  return (wd.dy >= 0) ? Rotation::SOUTH : Rotation::NORTH;
}

void RestaurantView::face_chair_towards_nearest_table_if_any(
    const WorldRect& candidate, const PlacedFurniture* nearest_table,
    const WorldRect& table_rect) {
  if (!nearest_table) {
    return;
  }
  if (on_cursor_ &&
      on_cursor_->furniture()->category() == FurnitureCategory::CHAIR) {
    on_cursor_->rotate(rotation_towards(candidate, table_rect));
  }
}

void RestaurantView::place_cursor_item(WorldPoint wp) {
  if (!on_cursor_) {
    return;
  }

  // wp is already the top-left corner (calculated by candidate_rect)
  // Store it directly as the furniture origin
  on_cursor_->origin(wp);

  if (kitchen_view_) {
    restaurant_->layout()->place_kitchen_furniture(std::move(on_cursor_));
  }
  else {
    restaurant_->layout()->place_furniture(std::move(on_cursor_));
  }

  state_.advance_time_blocks(1);

  // Clear ghost rendering state
  ghost_furniture_topleft_.reset();
  show_furniture_cursor_ = false;

  context_.cursor->clear_custom_cursor();
  on_cursor_.reset();
  mark_dirty();

  // Consume one from the batch of "to be placed" furniture; refresh or advance
  decrement_or_erase_current_and_maybe_advance();

  // Update buttons after placement (may restore normal buttons if batch is
  // empty)
  update_buttons_for_furniture_state();
}

bool RestaurantView::can_place_now(WorldPoint p, WorldRect& out_candidate,
                                   const PlacedFurniture*& out_nearest_table,
                                   WorldRect& out_nearest_table_rect) const {
  if (!on_cursor_) {
    return false;
  }

  out_candidate = candidate_rect(p, *on_cursor_);

  if (intersects_wall(out_candidate)) {
    return false;
  }

  if (intersects_any_blocking_furniture(out_candidate, *on_cursor_,
                                        out_nearest_table,
                                        out_nearest_table_rect)) {
    return false;
  }
  return true;
}

void RestaurantView::render(const RenderContext& ctx) {
  ctx.surface->fill(14); // TODO: figure out proper background color
  interactions_.begin_frame();
  ctx.surface->palette({4});

  if (view_mode_ == RestaurantViewMode::SABOTAGE) {
    render_sabotage(ctx);
    return;
  }

  calculate_background_dimensions();
  prepare_background();

  render_top_wall_and_floor();

  render_scene_passes();

  if (on_cursor_ && ghost_furniture_topleft_) {
    render_furniture_ghost();
  }

  // side and bottom walls (rendered last to cover legacy furniture in
  // illegal positions, e.g. one of the walls is placed into the wall in one of
  // the on disk restaurant layouts)
  render_side_and_bottom_walls();

  // Blit the visible portion of background_ to screen.
  // background_ is rendered at 1:1, so world coords are pixel indices into it.
  const auto [x, y, w, h] = camera_.visible_world_rect();
  const ViewRect src_rect{x, y, w, h};
  const ViewPoint dst_point = camera_.origin_offset_in_view();

  ctx.surface->blit(background_, src_rect, dst_point);
}

void RestaurantView::render_top_wall_and_floor() {
  constexpr int tile_w = RESTAURANT_TILE_WIDTH;
  constexpr int tile_h = RESTAURANT_TILE_HEIGHT;

  const auto room_size =
      kitchen_view_ ? restaurant_->kitchen_size() : restaurant_->size();
  const int interior_cols = room_size.w;
  const int interior_rows = room_size.h;

  // Tile source rectangles
  const ViewRect horiz_wall_src = {HORIZONTAL_WALL_TILE_OFFSET * tile_w, 0,
                                   tile_w, tile_h};
  const ViewRect floor_base_src = {FLOOR_TILE_OFFSET * tile_w, 0, tile_w,
                                   tile_h};

  // Floor pattern
  const auto pattern = restaurant_->layout()->floor_pattern();
  const std::string k_filename =
      compat::format("gfx/k{}", pattern.vendor_id + 1);
  const auto& floor_surface = context_.vga->get(k_filename);

  // World coordinates: walls start at (0, 0), interior at (tile_w, tile_h)

  // top wall
  for (int col = 0; col < interior_cols + 2; ++col) {
    const int x = col * tile_w;
    const int y = 0;
    background_.blit(k0_, horiz_wall_src, ViewPoint{x, y});
  }

  // interior floor tiles
  for (int row = 0; row < interior_rows; ++row) {
    const int y = (row + 1) * tile_h; // +1 to skip top wall

    for (int col = 0; col < interior_cols; ++col) {
      const int x = (col + 1) * tile_w; // +1 to skip left wall

      if (kitchen_view_) {
        // Kitchen: single base tile, no pattern
        background_.blit(k0_, floor_base_src, ViewPoint{x, y});
      }
      else {
        // Restaurant: 2x2 pattern with shadow variant
        const bool even_row = (row & 1) == 0;
        const bool even_col = (col & 1) == 0;
        const std::size_t idx =
            even_row ? (even_col ? 0 : 1) : (even_col ? 2 : 3);

        const uint8_t tile_index = pattern.tile_indices[idx];
        const bool in_shadow = (row < 2) || (col >= interior_cols - 2);
        const int src_y_off = in_shadow ? tile_h : 0;

        const ViewRect src{static_cast<int>(tile_index) * tile_w, src_y_off,
                           tile_w, tile_h};
        background_.blit(floor_surface, src, ViewPoint{x, y});
      }
    }
  }
}

void RestaurantView::render_side_and_bottom_walls() {
  constexpr int tile_w = RESTAURANT_TILE_WIDTH;
  constexpr int tile_h = RESTAURANT_TILE_HEIGHT;

  const auto room_size =
      kitchen_view_ ? restaurant_->kitchen_size() : restaurant_->size();
  const int interior_cols = room_size.w;
  const int interior_rows = room_size.h;

  // Tile source rectangles
  const ViewRect brown_tile_src = {BROWN_TILE_OFFSET * tile_w, 0, tile_w,
                                   tile_h};
  const ViewRect horiz_wall_src = {HORIZONTAL_WALL_TILE_OFFSET * tile_w, 0,
                                   tile_w, tile_h};
  const ViewRect vert_wall_src = {VERTICAL_WALL_TILE_OFFSET * tile_w, 0, tile_w,
                                  tile_h};
  const ViewRect wood_tile_src = {WOOD_TILE_OFFSET * tile_w, 0, tile_w, tile_h};
  const ViewRect floor_base_src = {FLOOR_TILE_OFFSET * tile_w, 0, tile_w,
                                   tile_h};

  // side walls
  const int entrance_row_offset = restaurant_->layout()->kitchen_entry_offset();
  const int first_section_end = entrance_row_offset + KITCHEN_ENTRANCE_ROWS;
  const int second_section_end = first_section_end + KITCHEN_ENTRANCE_ROWS;

  for (int row = 0; row < interior_rows; ++row) {
    const int y = (row + 1) * tile_h; // +1 to skip top wall

    // Left wall / divider
    ViewRect left_tile = vert_wall_src;
    if (!kitchen_view_) {
      // Restaurant view: left wall has entrance to kitchen
      if (row < entrance_row_offset) {
        left_tile = vert_wall_src;
      }
      else if (row < first_section_end) {
        left_tile = floor_base_src; // door opening
      }
      else if (row < second_section_end) {
        left_tile = wood_tile_src; // door frame
      }
    }
    background_.blit(k0_, left_tile, ViewPoint{0, y});

    // Right wall / divider
    ViewRect right_tile = vert_wall_src;
    if (kitchen_view_) {
      // Kitchen view: right wall has entrance to restaurant
      if (row < entrance_row_offset) {
        right_tile = vert_wall_src;
      }
      else if (row < first_section_end) {
        right_tile = floor_base_src; // door opening
      }
      else if (row < second_section_end) {
        right_tile = wood_tile_src; // door frame
      }
    }
    const int right_x = (interior_cols + 1) * tile_w;
    background_.blit(k0_, right_tile, ViewPoint{right_x, y});
  }

  // bottom wall
  const int entrance_offset = restaurant_->layout()->entrance_offset();
  const int entrance_end = entrance_offset + 3;
  const int bottom_y = (interior_rows + 1) * tile_h;

  for (int col = 0; col < interior_cols + 2; ++col) {
    const int x = col * tile_w;
    const bool is_entrance =
        !kitchen_view_ && col >= entrance_offset && col < entrance_end;
    const ViewRect& tile = is_entrance ? brown_tile_src : horiz_wall_src;
    background_.blit(k0_, tile, ViewPoint{x, bottom_y});
  }

  // Render the customer entrance door
  if (!kitchen_view_ && entrance_offset >= 0) {
    Bob& door_bob = context_.bob->get("bob/moebel.bob");

    // frame 59 = closed door, frame 60 = open door
    int door_frame =
        restaurant_->is_open(state_.time().time_of_day()) ? 60 : 59;
    Surface door_surface = door_bob.frame(door_frame, k0_.palette());

    // position door at entrance
    int door_x = (entrance_offset * tile_w);
    int door_y = bottom_y;

    if (restaurant_->is_open(state_.time().time_of_day())) {
      // Open door: show top TILE_HEIGHT pixels starting at x=4 (hinge offset)
      ViewRect src_rect{4, 0, door_surface.width(), RESTAURANT_TILE_HEIGHT};
      background_.blit(door_surface, src_rect, ViewPoint{door_x + 4, door_y});
    }
    else {
      // Closed door
      background_.blit_masked(door_surface, ViewPoint{door_x, door_y});
    }
  }

  // door interaction
  // The dividing wall door can be clicked to toggle between restaurant and
  // kitchen views In restaurant view: door is on left wall (entrance to
  // kitchen) In kitchen view: door is on right wall (entrance to restaurant)

  const int door_x = kitchen_view_ ? ((interior_cols + 1) * tile_w) : 0;
  const int door_y = (entrance_row_offset + 1) *
                     tile_h; // +1 because interior rows start after top wall
  const int door_height =
      (KITCHEN_ENTRANCE_ROWS * 2) * tile_h; // Entrance spans 6 rows total

  const WorldRect door_world{door_x, door_y, tile_w, door_height};

  // Convert world coordinates to viewport-local coordinates and register
  // interaction
  if (auto door_viewport = map_rect_to_viewport(door_world, camera_)) {
    const char view_ch = kitchen_view_ ? 'K' : 'R';
    const std::string token_id = compat::format("{}_door", view_ch);

    interactions_.create_frame_item(
        *door_viewport, token_id, [this](views::ui::InteractionEvent ev) {
          if (ev.type != views::ui::InteractionEvent::Type::Click) {
            return;
          }
          // Don't toggle kitchen view if holding furniture - let catchall place
          // it instead
          if (on_cursor_) {
            return;
          }
          toggle_kitchen_view();
        });
  }
}

// =============================================================================
// FURNITURE GHOST RENDERING (when holding furniture)
// =============================================================================

WorldPoint
RestaurantView::clamp_furniture_to_interior(WorldPoint mouse_world) const {
  const auto sz = current_bob_renderer().size(*on_cursor_);
  const auto room_size =
      kitchen_view_ ? restaurant_->kitchen_size() : restaurant_->size();

  constexpr int tile_w = RESTAURANT_TILE_WIDTH;
  constexpr int tile_h = RESTAURANT_TILE_HEIGHT;

  // Interior bounds in world coordinates
  // Furniture top-left must be at least at (tile_w, tile_h) and at most
  // so that bottom-right doesn't exceed the interior
  const int interior_min_x = tile_w;
  const int interior_min_y = tile_h;
  const int interior_max_x = (room_size.w + 1) * tile_w - sz.w;
  const int interior_max_y = (room_size.h + 1) * tile_h - sz.h;

  // Get the visible world rect from the camera
  const WorldRect visible = camera_.visible_world_rect();

  // Clamp to the visible portion of the interior
  // The furniture top-left can be anywhere in the visible viewport,
  // but not beyond the interior bounds
  // We ensure the furniture stays fully within the visible area by
  // subtracting the furniture size from the max bounds
  const int min_x = std::max(interior_min_x, visible.x);
  const int min_y = std::max(interior_min_y, visible.y);
  const int max_x = std::min(interior_max_x, visible.x + visible.w - sz.w);
  const int max_y = std::min(interior_max_y, visible.y + visible.h - sz.h);

  // Ensure max is never less than min (can happen with very large furniture)
  const int final_max_x = std::max(min_x, max_x);
  const int final_max_y = std::max(min_y, max_y);

  return WorldPoint{std::clamp(mouse_world.x, min_x, final_max_x),
                    std::clamp(mouse_world.y, min_y, final_max_y)};
}

void RestaurantView::render_furniture_ghost() {
  // Only render ghost when cursor is hidden (outside viewport, frozen state)
  if (!on_cursor_ || !ghost_furniture_topleft_ || show_furniture_cursor_) {
    return;
  }

  const auto& bob_surface = current_bob_renderer().image(*on_cursor_);

  // Render furniture ghost at the clamped position (frozen)
  ViewPoint dst{ghost_furniture_topleft_->x, ghost_furniture_topleft_->y};
  background_.blit_masked(bob_surface, dst);
}

void RestaurantView::update_buttons_for_furniture_state() {
  input_panel_->delete_buttons();

  if (on_cursor_ && !return_to_supplier_type_) {
    // Pick up existing furniture mode
    input_panel_->add_button(Icon::CUP, ButtonType::Horizontal,
                             ButtonPosition::TopLeft, "Throw away",
                             [this]() { handle_throw_away(); });

    input_panel_->add_button(Icon::TRUCK, ButtonType::Horizontal,
                             ButtonPosition::MiddleLeft, "Move elsewhere",
                             [this]() { handle_move_elsewhere(); });

    input_panel_->add_button(Icon::MONEY_BAG, ButtonType::Horizontal,
                             ButtonPosition::BottomLeft, "Sell it",
                             [this]() { handle_sell_furniture(); });
  }
  else if (on_cursor_ && return_to_supplier_type_) {
    // Buying new furniture
    input_panel_->add_button(Icon::ARROW_UP, ButtonType::Horizontal,
                             ButtonPosition::TopLeft, "Next item",
                             [this]() { handle_next_item(); });

    input_panel_->add_button(Icon::ARROW_DOWN, ButtonType::Horizontal,
                             ButtonPosition::MiddleLeft, "Previous item",
                             [this]() { handle_previous_item(); });

    input_panel_->add_button(Icon::CUP, ButtonType::Vertical,
                             ButtonPosition::TopRight, "Throw away",
                             [this]() { handle_sell_furniture(); });
  }
  else {
    // Normal mode: restore default buttons
    set_up_buttons();
  }
}

void RestaurantView::handle_throw_away() {
  if (!on_cursor_) {
    return;
  }

  throw_away_one_from_current_batch();
  update_buttons_for_furniture_state();
}

void RestaurantView::handle_move_elsewhere() {
  if (!on_cursor_ || !active_batch_index_) {
    return;
  }

  // Add current furniture to the global placement queue for other restaurants
  auto* batch = current_batch();
  if (batch && batch->furniture) {
    // Move one item from current batch to global queue
    // Preserve rotation if it was stored
    FurnitureBatch move_batch{batch->furniture, 1, batch->bob_state};
    state_.restaurant_placement().push_back(move_batch);

    // Decrement current batch without auto-advancing
    // (prevents immediately picking up the furniture we just moved)
    decrement_or_erase_current_and_maybe_advance(false);
    update_buttons_for_furniture_state();
    mark_dirty();
  }
}

void RestaurantView::handle_sell_furniture() {
  if (!on_cursor_) {
    return;
  }

  const auto* furn = on_cursor_->furniture();
  Money sell_price = furn->price() / 2;

  auto do_sell = [this, sell_price]() {
    auto* player = state_.active_player();
    Money full_price = sell_price * 2;
    Money loss = full_price - sell_price;

    // Credit cash with the sell price
    player->financials().transfer(state_.time().current_date(),
                                  Account::Furniture, Account::Cash, sell_price,
                                  AccountingScope::Operational);
    // Write off the lost value
    player->financials().transfer(state_.time().current_date(),
                                  Account::Furniture, Account::ProfitAndLoss,
                                  loss, AccountingScope::Operational);

    decrement_or_erase_current_and_maybe_advance();
    update_buttons_for_furniture_state();
    mark_dirty();
  };

  // Check if confirmation is required
  if (!state_.options().qol_improvements) {
    auto msg =
        compat::format("Do you want to sell this|item for {:w}?", sell_price);
    context_.popup->origin(ViewPoint{52, 80});
    context_.popup->create<ConfirmPopup>(msg, [do_sell](bool confirmed) {
      if (confirmed) {
        do_sell();
      }
    });
  }
  else {
    do_sell();
  }
}

void RestaurantView::check_placement_complete_and_navigate_back() {
  // Only navigate back if all furniture has been placed AND we have a return_to
  // destination
  if (furniture_placement_.empty() && return_to_supplier_type_.has_value()) {
    // Replace our own state so that when the user dismisses the purchasing
    // screen, the restaurant view is reconstructed in normal mode with the
    // restaurant re-opened.
    state_.ui().replace_top<RestaurantViewState>(RestaurantViewState{
        .id = restaurant_->id(),
        .mode = RestaurantViewMode::PLAYER_DEFAULT,
        .reopen_restaurant = true,
    });
    state_.ui().push<CategoryOverviewState>(
        CategoryOverviewState{.type = *return_to_supplier_type_});
  }
}

void RestaurantView::zoom_out_of_restaurant_view() {
  // go back to CityView
  if (state_.active_city_id().value == -1) {
    state_.active_city(core::CityId{0}); // debug fallback
  }
  state_.ui().push<CityViewState>(state_.active_city_id());
}

void RestaurantView::handle_next_item() {
  if (furniture_placement_.empty()) {
    return;
  }

  const size_t start = active_batch_index_.value_or(0);
  const size_t n = furniture_placement_.size();

  // Look for next batch with count > 0 (wrapping around)
  for (size_t step = 1; step < n; ++step) {
    const size_t i = (start + step) % n;
    if (furniture_placement_[i].count > 0) {
      set_active_batch(i);
      mark_dirty();
      return;
    }
  }
}

void RestaurantView::handle_previous_item() {
  if (furniture_placement_.empty()) {
    return;
  }

  const size_t start = active_batch_index_.value_or(0);
  const size_t n = furniture_placement_.size();

  // Look for previous batch with count > 0 (wrapping around backwards)
  for (size_t step = 1; step < n; ++step) {
    const size_t i = (start + n - step) % n;
    if (furniture_placement_[i].count > 0) {
      set_active_batch(i);
      mark_dirty();
      return;
    }
  }
}

void RestaurantView::update_status_display(const PlacedFurniture* pf) {
  auto hub = state_.status();
  StatusDisplayRecipe recipe;
  if (!pf) {
    recipe.add_single("nothing");
  }
  else if (kitchen_view_) {
    const auto* furniture = pf->furniture();
    auto category = furniture->category();
    if (category != FurnitureCategory::KITCHENWARE) {
      fatal("Invalid category: How did you get this in the kitchen!?");
    }
    recipe.add_single(furniture->name());
    recipe.add_single(compat::format("quality: {}", furniture->quality()));
    auto bake_time = furniture->bake_time();
    auto capacity = furniture->capacity();
    int pizzas_per_hour = 60 / bake_time * capacity;
    recipe.add_single(compat::format("{} pizzas at a time", capacity));
    recipe.add_single(compat::format("{} pizzas per hour", pizzas_per_hour));
    recipe.add_single(compat::format("{} minutes per pizza", bake_time));
    recipe.add_single(compat::format("price {:w}", furniture->price()));
  }
  else {
    const auto* furniture = pf->furniture();
    const auto category = furniture->category();
    if (category == FurnitureCategory::OTHER) {
      recipe.add_single("nothing");
    }
    else {
      recipe.add_single(to_string(category));
    }
    recipe.add_single(furniture->name());
    recipe.add_single(compat::format("quality: {}", furniture->quality()));
    recipe.add_single(compat::format("price {:w}", furniture->price()));

    const auto& layout = restaurant_->layout();
    if (category == FurnitureCategory::TABLE) {
      int count = layout->linked_furniture_count(pf->id());
      auto linked = pf->linked_furniture_id();
      recipe.add_single(
          compat::format("room for {} people", furniture->capacity()));
      recipe.add_single(compat::format("with {} chairs", count));
    }
    else if (category == FurnitureCategory::CHAIR) {
      auto linked_id = pf->linked_furniture_id();
      if (linked_id) {
        auto* linked = layout->find_by_id(*linked_id, false);
        if (linked) {
          recipe.add_single("is at table");
          recipe.add_single(linked->furniture()->name());
        }
      }
    }
  }

  StatusMsg msg{
      .payload = recipe,
      .scope = StatusScope::Restaurant,
      .type = TransientType::Ephemeral,
      .ttl = std::nullopt,
      .coalesce_key = "furniture_reposition_close",
  };
  hub.post(msg);
}

void RestaurantView::render_sabotage(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back14"));

  const int text_color = 240;
  const ViewPoint which_action_pos{148, 134};
  context_.font->render_text_onto(*ctx.surface, "which action?",
                                  which_action_pos, FontFace::PIZ_NUMB,
                                  text_color);

  auto* player = state_.active_player();
  auto& inventory = player->inventory();

  int y_position = 164;
  for (auto item : core::item::sabotage_list) {
    auto bag_count = inventory.count(item);

    // Create hover box at current y_position (relative to text_box)
    ViewRect hover_box{122, y_position, 128, 18};

    // Draw background ONLY if this weapon is hovered
    if (hovered_sabotage_item_ && hovered_sabotage_item_ == item) {
      ctx.surface->fill_rect(hover_box, {15});
    }

    auto name = to_string(item);
    // Text positioned 1px down for vertical centering (1px + text + 1px)
    int text_y = y_position + 1;
    context_.font->render_text_onto(*ctx.surface, name, {124, text_y},
                                    FontFace::PIZ_NUMB, text_color);
    context_.font->render_text_onto(*ctx.surface, std::to_string(bag_count),
                                    {292, text_y}, FontFace::PIZ_NUMB,
                                    text_color, AlignX::RightEdge);

    interactions_.create_frame_item(
        hover_box, name,
        [this, name, item, bag_count](views::ui::InteractionEvent ev) {
          if (ev.type == views::ui::InteractionEvent::Type::Enter) {
            hovered_sabotage_item_ = item;
            mark_dirty();
          }
          else if (ev.type == views::ui::InteractionEvent::Type::Leave) {
            hovered_sabotage_item_ = std::nullopt;
            mark_dirty();
          }
          else if (ev.type == views::ui::InteractionEvent::Type::Click) {
            if (ev.button == views::ui::MouseButton::Left) {
              if (bag_count > 0) {
                Logger::debug("Attack with {}", std::string(name));
              }
              // Move from depot to bag (as long as depot > 0 and bag < 255)
            }
          }
        });

    y_position += 20;
  }
}
