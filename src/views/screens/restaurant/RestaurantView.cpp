/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/restaurant/RestaurantView.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include "core/Demographics.h"
#include "core/GameTypes.h"
#include "core/character/Player.h"
#include "core/city/District.h"
#include "core/city/Property.h"
#include "core/factories/PizzaFactory.h"
#include "core/finance/Money.h"
#include "core/item/ActionText.h"
#include "core/restaurant/Furniture.h"
#include "core/restaurant/RestaurantTypes.h"
#include "core/state/ClockRate.h"
#include "core/state/CompetitionSession.h"
#include "core/state/GameState.h"
#include "core/state/InterruptionRate.h"
#include "core/status/StatusHub.h"
#include "core/util/LegacyConstants.h"
#include "engine/Fatal.h"
#include "engine/FormatCompat.h"
#include "engine/Point.h"
#include "engine/Random.h"
#include "engine/audio/AudioManager.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/Blit.h"
#include "engine/ui/ICursorManager.h"
#include "engine/ui/InteractionEvent.h"
#include "resources/BobSizeFn.h"
#include "views/base/BobRenderer.h"
#include "views/base/ViewDefs.h"
#include "views/screens/city_view/ZoomConfig.h"
#include "views/screens/restaurant/GuestPicker.h"
#include "views/screens/restaurant/RestaurantStats.h"
#include "views/screens/restaurant/WalkNavigation.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/AlertPopup.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/ui/popups/GuestTalkPopup.h"
#include "views/ui/popups/JukeboxPopup.h"
#include "views/ui/popups/NumberInputPopup.h"
#include "views/ui/popups/TvPopup.h"

using namespace views::base;
using namespace views::screens::restaurant;
using namespace LegacyConstants;

static constexpr int screen_charge_blocks = 1;
static constexpr int staff_screen_charge_blocks = 2;

RestaurantView::RestaurantView(GameContext& context, GameState& state,
                               RestaurantViewState r_state)
    : MainPanelView(context, state), k0_(context_.vga->get("gfx/k0")),
      furniture_renderer_(BobRenderer("bob/moebel.bob", context_.bob)),
      oven_renderer_(BobRenderer("bob/kuebobs.bob", context_.bob)),
      pizza_bob_(context_.bob->get("bob/moebel.bob")),
      restaurant_(state_.restaurant(r_state.id)),
      // will be properly sized later
      background_{Size{1, 1}},
      view_mode_(state_.restaurant_placement().empty()
                     ? r_state.mode
                     : RestaurantViewMode::PLAYER_NEW_FURNITURE),
      camctrl_{context.input, camera_}, status_controller_(state.status()),
      listener_{context_.input->register_listener(*this)} {
  assert(restaurant_);

  placement_.set_return_destination(r_state.return_to_supplier_type);

  // Set this restaurant as the active one so other views know which restaurant
  // we're working with
  state_.active_restaurant(r_state.id);

  if (const int blocks = state_.take_pending_screen_charge(); blocks > 0) {
    state_.advance_time_blocks(blocks);
  }

  kitchen_view_ = r_state.kitchen_view;

  // Check if we're placing ovens. If so, start in kitchen view.
  // Also check if we're placing regular furniture while the restaurant is open,
  // in which case we need to close it temporarily.
  bool post_waiting_customers_msg = false;
  if (!state_.restaurant_placement().empty()) {
    for (const auto& batch : state_.restaurant_placement()) {
      if (batch.furniture &&
          batch.furniture->category() == FurnitureCategory::KITCHENWARE) {
        kitchen_view_ = true;
        break;
      }
    }
    if (!kitchen_view_ && restaurant_->wants_open()) {
      restaurant_->close_for_business();
      restaurant_->add_reputation_damage(-16, 16);
      closed_for_placement_ = true;
      post_waiting_customers_msg = true;
    }
  }

  if (r_state.reopen_restaurant) {
    restaurant_->open_for_business(state_);
  }

  if (view_mode_ == RestaurantViewMode::OPPONENT_VIEW &&
      !restaurant_->is_within_business_hours(state_.time().time_of_day())) {
    view_mode_ = RestaurantViewMode::WEAPON_SELECT;
  }

  set_up_buttons();

  scroll_edges_.add_interactions(interactions_);

  // Calculate background dimensions based on restaurant/kitchen size
  calculate_background_dimensions();
  rmb_drag_scroller_.init(
      camctrl_, [this] { return state_.options().qol_improvements; },
      [this] { return state_.options().rmb_drag_inverted; },
      [this](ViewPoint) {
        if (!placement_.rotate_cursor()) {
          zoom_out_of_restaurant_view();
        }
      },
      [this](bool lock) { context_.cursor->set_relative_mouse_mode(lock); });
  register_interactions_viewport();
  if (view_mode_ == RestaurantViewMode::PLAYER_NEW_FURNITURE) {
    placement_.init_active_batch_if_needed();
  }

  // Update buttons based on whether we have furniture to place
  placement_.update_buttons();

  // Set up camera with proper world size. Alignment handles small rooms;
  // scroll handles large ones.
  camera_.view_size(Size{MAIN_PANEL_WIDTH, MAIN_PANEL_HEIGHT});
  apply_view_camera();

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
    state_.status().post_direct(StatusMsg{
        .payload = "The restaurant is closed while the furniture is being "
                   "positioned. There are some customers waiting impatiently "
                   "outside!",
        .scope = StatusScope::Restaurant,
    });
  }

  if (const auto music = restaurant_->jukebox_music(); music > 0) {
    context_.audio->play_music(static_cast<audio::MusicId>(music - 1));
  }

  update_restaurant_noise();
}

InterruptionRate RestaurantView::interruption_rate() const {
  // In the original, an attack and furniture placement never reach the
  // dispatcher.
  if (attack_.active() || !state_.restaurant_placement().empty()) {
    return InterruptionRate::None;
  }
  return InterruptionRate::Half;
}

ClockRate RestaurantView::clock_rate() const {
  return attack_.active() ? ClockRate::SecondsOnly : ClockRate::Running;
}

RestaurantView::~RestaurantView() {
  attack_.finish_scene();
  context_.audio->stop_music();
  if (current_restaurant_noise_) {
    context_.audio->stop_sound(*current_restaurant_noise_);
  }
}

void RestaurantView::update_restaurant_noise() {
  if (!context_.audio || !state_.options().restaurant_fx) {
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
  context_.audio->play_sound(noise, audio::LoopMode::Loop);
}

bool RestaurantView::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }

  const auto current_id = restaurant_->id();
  const auto owner_pid = restaurant_->property().owner_or_tenant();
  if (!owner_pid) {
    fatal("My current restaurant is not owned by me?!");
  }

  auto push_view = [&](core::RestaurantId rid) {
    // Update the stored city view scroll to center on the new restaurant
    auto* rest = state_.restaurant(rid);
    if (rest) {
      auto cvs = state_.city_view_view_state();
      center_city_view_on_index(cvs, rest->property().index());
      state_.city_view_view_state(cvs);
    }

    state_.ui().replace_top<RestaurantViewState>(RestaurantViewState{
        .id = rid,
        .mode = view_mode_,
        .kitchen_view = kitchen_view_,
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
    if (in_player_mode()) {
      toggle_kitchen_view();
      mark_dirty();
    }
    return true;
  }
  case input::Action::Fire: {
    // Firing is handled per tick in AttackScene::tick(); but just consume the
    // event here so it does not fall through
    if (attack_.active()) {
      return true;
    }
    break;
  }
  case input::Action::P2_Fire: {
    if (attack_.active() && attack_.has_defender()) {
      return true;
    }
    break;
  }
#ifndef NDEBUG
  case input::Action::Backspace: {
    walk_scene_.toggle_walk_debug();
    mark_dirty();
    return true;
  }
#endif
  default: {
    break;
  }
  }

  return false;
}

void RestaurantView::set_up_buttons() {
  input_panel_->delete_buttons();
  if (view_mode_ == RestaurantViewMode::OPPONENT_VIEW) {
    auto opp_pid_opt = restaurant_->property().owner_or_tenant();
    if (!opp_pid_opt) {
      input_panel_->add_button(Icon::CHEFS_HAT, ButtonType::Horizontal,
                               ButtonPosition::TopLeft, "Challenge", [] {});
    }
    else {
      core::PlayerId opp_pid = *opp_pid_opt;
      input_panel_->add_button(
          Icon::CHEFS_HAT, ButtonType::Horizontal, ButtonPosition::TopLeft,
          "Challenge", [this, opp_pid] { challenge_check_and_start(opp_pid); });
    }

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
  if (view_mode_ == RestaurantViewMode::SABOTAGE ||
      view_mode_ == RestaurantViewMode::WEAPON_SELECT) {
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
                  restaurant_->close_for_business();
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
                  restaurant_->open_for_business(state_);
                  update_restaurant_noise();
                  if (auto* player = state_.active_player()) {
                    state_.milestones().check_restaurant_milestones(*player);
                  }
                }

                mark_dirty();
              });
        }
      });

  input_panel_->add_button(Icon::STOCK, ButtonType::Square,
                           ButtonPosition::TopCenter, "Stock", [this]() {
                             state_.pending_screen_charge(screen_charge_blocks);
                             state_.ui().push<StockMainState>();
                           });

  input_panel_->add_button(Icon::MENU, ButtonType::Square,
                           ButtonPosition::TopRight, "Menu", [this]() {
                             assert(restaurant_);
                             state_.pending_screen_charge(screen_charge_blocks);
                             state_.ui().push<RestaurantMenuState>(
                                 restaurant_->id());
                           });

  input_panel_->add_button(Icon::FURNISHING, ButtonType::Square,
                           ButtonPosition::MiddleLeft, "Furnishing", [this]() {
                             state_.pending_screen_charge(screen_charge_blocks);
                             state_.ui().push<PurchasingState>(
                                 SupplierType::FURNITURE);
                           });

  input_panel_->add_button(
      Icon::CHARACTER2, ButtonType::Square, ButtonPosition::MiddleCenter,
      "Staff", [this]() {
        state_.pending_screen_charge(staff_screen_charge_blocks);
        state_.ui().push_screen(ScreenTag::Kind::StaffView);
      });

  input_panel_->add_button(
      Icon::PARCHMENT, ButtonType::Square, ButtonPosition::MiddleRight,
      "Recipe list", [this]() {
        state_.pending_screen_charge(screen_charge_blocks);
        state_.ui().push<PizzaDesignState>(PizzaDesignMode::Recipes);
      });

  input_panel_->add_button(
      Icon::GRAPH, ButtonType::Square, ButtonPosition::BottomLeft,
      "Restaurant stats", [this]() {
        state_.pending_screen_charge(screen_charge_blocks);
        state_.ui().push<RestaurantStatsState>(restaurant_->id());
      });

  input_panel_->add_button(Icon::COMPUTER, ButtonType::Square,
                           ButtonPosition::BottomCenter, "Terminal", [this]() {
                             state_.pending_screen_charge(screen_charge_blocks);
                             state_.ui().push<TerminalState>();
                           });

  input_panel_->add_button(
      Icon::ARROW_UP_DOWN, ButtonType::Square, ButtonPosition::BottomRight,
      "Animated stats", [this]() {
        state_.pending_screen_charge(screen_charge_blocks);
        state_.ui().push<AnimatedStatsState>(restaurant_->id());
      });
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
          if (!state_.options().qol_improvements &&
              !placement_.rotate_cursor()) {
            zoom_out_of_restaurant_view();
          }
          return;
        }
        // Left click handling
        if (view_mode_ == RestaurantViewMode::OPPONENT_VIEW ||
            view_mode_ == RestaurantViewMode::OPPONENT_ATTACK) {
          return;
        }
        if (placement_.holding() && placement_.ghost_topleft()) {
          // When holding furniture, always try to place at ghost position
          // This takes precedence over other interactions (door, etc.)
          handle_mouse_click(*placement_.ghost_topleft());
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
        if (!placement_.holding()) {
          return; // Only process if holding furniture
        }

        switch (ev.type) {
        case views::ui::InteractionEvent::Type::Enter:
          // Don't automatically show cursor on enter - let the next Hover event
          // determine whether to show cursor or ghost based on clamping
          break;

        case views::ui::InteractionEvent::Type::Hover:
          // Update ghost position as mouse moves
          placement_.update_ghost(camera_.view_to_world(ev.local));
          break;

        case views::ui::InteractionEvent::Type::Leave:
          // Disable custom cursor when leaving viewport
          placement_.hide_cursor();
          mark_dirty();
          break;

        case views::ui::InteractionEvent::Type::Scroll:
          // Item cycling via scroll wheel lives here rather than in the
          // catchall because furniture frame items steal the normal-layer
          // scroll event when the mouse is over placed furniture, and they
          // ignore scroll entirely. Background items receive scroll
          // regardless, so cycling works everywhere in the viewport.
          if (placement_.buying_mode()) {
            if (ev.scroll_delta > 0) {
              placement_.handle_next_item();
            }
            else if (ev.scroll_delta < 0) {
              placement_.handle_previous_item();
            }
          }
          break;

        default:
          break;
        }
      },
      {.layer = views::ui::InteractionLayer::Background});

  rmb_drag_scroller_.add_interaction(interactions_, vp_local,
                                     [this] { mark_dirty(); });
}

void RestaurantView::handle_mouse_click(WorldPoint map_point) {
  if (placement_.holding()) {
    placement_.try_place_at(map_point);
  }
  else {
    update_status_display(nullptr);
  }
}

void RestaurantView::toggle_kitchen_view() {
  (kitchen_view_ ? kitchen_scroll_ : restaurant_scroll_) = camera_.scroll();
  kitchen_view_ = !kitchen_view_;

  // Recalculate background size for the new view
  calculate_background_dimensions();
  apply_view_camera();
  if (const auto& saved =
          kitchen_view_ ? kitchen_scroll_ : restaurant_scroll_) {
    camera_.scroll(*saved);
  }

  camctrl_.reset();
  mark_dirty();
}

void RestaurantView::apply_view_camera() {
  camera_.world_size(Size{background_width_, background_height_});

  // Set alignment based on view:
  // - Restaurant view: bottom-left (Start, End) to show entrance at bottom
  // - Kitchen view: bottom-right (End, End) matching original right+bottom
  // align
  if (kitchen_view_) {
    camera_.align(Camera::Align::End, Camera::Align::End);
    // Scroll to show bottom-right of kitchen (initial view in original)
    const int init_x = std::max(0, background_width_ - MAIN_PANEL_WIDTH);
    const int init_y = std::max(0, background_height_ - MAIN_PANEL_HEIGHT);
    camera_.scroll(WorldPoint{init_x, init_y});
  }
  else {
    camera_.align(Camera::Align::Start, Camera::Align::End);
    // Start at bottom of restaurant (where entrance is)
    const int init_y = std::max(0, background_height_ - MAIN_PANEL_HEIGHT);
    camera_.scroll(WorldPoint{0, init_y});
  }
}

void RestaurantView::calculate_background_dimensions() {

  // Exact size of restaurant/kitchen including walls (+2 for walls on each
  // side)
  auto room_size =
      kitchen_view_ ? restaurant_->kitchen_size() : restaurant_->size();
  background_width_ = (room_size.w + 2) * RESTAURANT_TILE_WIDTH;
  background_height_ = (room_size.h + 2) * RESTAURANT_TILE_HEIGHT;
}

void RestaurantView::advance(uint32_t dt_ms) {
  if (attack_.active()) {
    attack_.advance(dt_ms);
  }
}

void RestaurantView::tick() {
  if (attack_.active()) {
    // In attack mode by default the arrow keys move the attacker, not the
    // camera, so we have to skip camctrl_ here since it's bound to the same
    // keys. The attack scene moves the camera when they get close to the edge
    // so regardless of key bindings we just return.
    return;
  }

  if (camctrl_.tick()) {
    mark_dirty();
  }

  // Tick guest animations
  if (restaurant_->is_open(state_.time().time_of_day()) && !kitchen_view_) {
    for (auto& seated : seated_guests_) {
      seated.controller.tick();
    }
    mark_dirty();
  }
  walk_scene_.tick();
}

void RestaurantView::handle_furniture_click(const PlacedFurniture* pf) {
  const auto prop_id = pf->id();
  if ((kitchen_view_ || !restaurant_->is_open(state_.time().time_of_day())) &&
      last_clicked_furniture_index_ &&
      *last_clicked_furniture_index_ == prop_id) {
    placement_.pick_up(prop_id);
    last_clicked_furniture_index_.reset();
    mark_dirty();
  }
  else if (restaurant_->is_open(state_.time().time_of_day()) &&
           last_clicked_furniture_index_ &&
           *last_clicked_furniture_index_ == prop_id) {
    const auto role = pf->furniture()->role();
    if (role == FurnitureRole::Jukebox) {
      context_.popup->origin({66, 72});
      context_.popup->create<JukeboxPopup>(restaurant_);
    }
    else if (role == FurnitureRole::Tv) {
      context_.popup->origin({66, 72});
      context_.popup->create<TvPopup>(restaurant_);
    }
    else {
      StatusMsg msg{
          .payload =
              "You can rearrange furniture only when the restaurant is closed!",
          .scope = StatusScope::Restaurant,
      };
      state_.status().post_direct(msg);
    }
  }
  else if (!placement_.holding()) {
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

  rebuild_guest_cache();

  // Seats first
  render_placement_layer(PlacementLayer::Seating);

  // Then the guest legs
  render_guest_legs();

  // Then the tables (can be glass/transparent)
  render_placement_layer(PlacementLayer::Tables);

  // Pizzas on tables
  render_guest_pizzas();

  // Guest upper body/arms
  render_guest_upper();

  // Rest of the furniture and stuff
  render_placement_layer(PlacementLayer::Top);
}

void RestaurantView::render_placement_layer(PlacementLayer placement) {
  const auto& pf_list = current_furniture_list();
  // Panel (view) rect for clipping
  const Size vsz = camera_.view_size();
  const ViewRect panel{0, 0, vsz.w, vsz.h};

  for (const auto& pf_uptr : pf_list) {
    const PlacedFurniture* pf = pf_uptr.get();
    if (pf->furniture()->placement_layer() != placement) {
      continue;
    }

    const auto& r = bob_renderer_for(*pf);
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
      const std::string token_id =
          compat::format("furn_{}_{}", kitchen_view_ ? 'K' : 'R', fid.value);

      interactions_.create_frame_item(
          hit, token_id,
          [this, fid, hit, furn_world](views::ui::InteractionEvent ev) {
            if (ev.type != views::ui::InteractionEvent::Type::Click) {
              return;
            }
            if (view_mode_ == RestaurantViewMode::OPPONENT_VIEW ||
                view_mode_ == RestaurantViewMode::OPPONENT_ATTACK) {
              return;
            }
            if (ev.button == views::ui::MouseButton::Left) {
              auto* pf = restaurant_->layout()->find_by_id(fid, kitchen_view_);
              if (!pf) {
                return;
              }
              if (placement_.holding() && placement_.ghost_topleft()) {
                // item-local -> panel-local (view space)
                const ViewPoint p_view{hit.x + ev.local.x, hit.y + ev.local.y};
                // panel-local (view) -> world space
                const WorldPoint click_world = camera_.view_to_world(p_view);
                placement_.click_with_cursor_on(pf, furn_world, click_world);
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
              if (!state_.options().qol_improvements &&
                  !placement_.rotate_cursor()) {
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
  for (const auto& g : guests_cache_) {
    ViewPoint anchor_view{g.torso_anchor_map.x, g.torso_anchor_map.y};
    guest_renderer_.render_lower_body(background_, anchor_view, g.pose);
  }
}

void RestaurantView::render_guest_upper() {
  for (const auto& g : guests_cache_) {
    ViewPoint anchor_view{g.torso_anchor_map.x, g.torso_anchor_map.y};
    guest_renderer_.render_upper_body(background_, anchor_view, g.pose);
  }
}

void RestaurantView::render_guest_pizzas() {
  // Pizza offsets original but at our 2x scale
  constexpr std::array<int, 4> pizza_offset_x = {4, -24, 2, 10};
  constexpr std::array<int, 4> pizza_offset_y = {6, 0, -28, 0};

  for (const auto& g : guests_cache_) {
    // Only render pizza if guest is eating
    if (!g.pose.eating) {
      continue;
    }

    int direction = static_cast<int>(g.pose.facing) & 3;
    ViewPoint pizza_pos{g.torso_anchor_map.x + pizza_offset_x[direction],
                        g.torso_anchor_map.y + pizza_offset_y[direction]};

    // Render pizza sprite (frame 0x154 from moebel.bob)
    Surface pizza_sprite = pizza_bob_.frame(0x154);
    background_.blit_masked(pizza_sprite, pizza_pos);
  }
}

// gaeste.bob contains 12 seated guest types
// Each type has 4 facing directions, so the frame index for any part is
// `frame = base_index_for(part) + guest_type * 4 + facing`
//
// The 12 types have two options per demographic, first we
// have demographic 0 to 5 for the first option, then 6 to 11 for the
// second option per demographic (so 0 and 6 are two options for the first
// demographic, etc).
//
// Portrait files are gfx/g0.vga through g23.vga, two per guest type:
// face_index = guest_type * 2 + (0 or 1) (chosen randomly at spawn)
//
// We determine guest_type and face_index when a guest is spawned
// and store it on the controller so sprite and portrait always match.
void RestaurantView::seed_seated_guests() {
  const auto [guests, seats] = restaurant_->guest_occupancy();
  if (guests <= 0 || seats <= 0) {
    return;
  }
  // A chair's chance to start occupied is the average occupancy.
  const int occupied_percent = guests * 100 / seats;
  const auto& customer_counts = restaurant_->economy().customer_counts();

  for (const auto& pf : restaurant_->layout()->placed_furniture()) {
    if (!pf || pf->furniture()->category() != FurnitureCategory::CHAIR ||
        !pf->linked_furniture_id().has_value()) {
      continue;
    }
    if (!engine::random::presentation::rand_percentage_chance(
            occupied_percent)) {
      continue;
    }
    const ConsumerType demo = pick_weighted_demographic(customer_counts);
    CharacterController controller;
    controller.set_customer_type(demo);
    // The crowd is already mid-visit, so most have a pizza.
    const int eating_percent =
        60 + engine::random::presentation::rand_max_n(25);
    controller.set_eating(
        engine::random::presentation::rand_percentage_chance(eating_percent));
    const int gt = pick_guest_type(demo);
    controller.set_guest_type(gt);
    controller.set_face_index(pick_face_index(gt));
    seated_guests_.push_back(SeatedGuest{pf->id(), controller});
  }
}

void RestaurantView::rebuild_guest_cache() {
  guests_cache_.clear();

  if (kitchen_view_ || !restaurant_->is_open(state_.time().time_of_day())) {
    seated_guests_.clear();
    guest_talked_to_.clear();
    guests_seeded_ = false;
    walk_scene_.reset();
    return;
  }

  if (!guests_seeded_) {
    seed_seated_guests();
    guests_seeded_ = true;
  }

  using namespace views::screens::restaurant;

  constexpr std::array<int, 4> seating_offset_x = {-4, -3, -3, 4};
  constexpr std::array<int, 4> seating_offset_y = {2, -5, -6, -9};

  for (auto it = seated_guests_.begin(); it != seated_guests_.end();) {
    const core::FurnitureId chair_id = it->chair_id;
    const PlacedFurniture* pf =
        restaurant_->layout()->find_by_id(chair_id, false);
    if (!pf) {
      it = seated_guests_.erase(it);
      continue;
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

    CharacterPose pose = it->controller.pose();
    pose.facing = guest_facing;
    guests_cache_.push_back(GuestRenderItem{
        .torso_anchor_map = guest_pos,
        .pose = pose,
        .chair_id = chair_id,
        .interacted_before = false,
        .face_index = it->controller.face_index(),
    });
    ++it;
  }
}

RestaurantView::SeatedGuest*
RestaurantView::seated_guest(core::FurnitureId chair_id) {
  for (auto& seated : seated_guests_) {
    if (seated.chair_id == chair_id) {
      return &seated;
    }
  }
  return nullptr;
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
        const auto& entry = entries[engine::random::presentation::rand_max_n(
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
          info.district_price_level =
              restaurant_->property().district().average_income_factor();
        }
      }

      context_.popup->origin({62, 60});
      context_.popup->create<GuestTalkPopup>(info, restaurant_);
      guest_talked_to_[chair_id] = true;
      return i;
    }
  }
  return std::nullopt;
}

const BobRenderer&
RestaurantView::bob_renderer_for(const PlacedFurniture& pf) const {
  return pf.furniture()->category() == FurnitureCategory::KITCHENWARE
             ? oven_renderer_
             : furniture_renderer_;
}

const std::vector<std::unique_ptr<PlacedFurniture>>&
RestaurantView::current_furniture_list() const {
  return kitchen_view_ ? restaurant_->layout()->placed_kitchen_furniture()
                       : restaurant_->layout()->placed_furniture();
}

void RestaurantView::render(const RenderContext& ctx) {
  ctx.surface->fill(14); // TODO: figure out proper background color
  interactions_.begin_frame();
  ctx.surface->palette({4});

  if (view_mode_ == RestaurantViewMode::SABOTAGE) {
    render_sabotage(ctx);
    return;
  }

  if (view_mode_ == RestaurantViewMode::WEAPON_SELECT) {
    render_weapon_select(ctx);
    return;
  }

  calculate_background_dimensions();
  prepare_background();

  render_top_wall_and_floor();

  render_scene_passes();

  placement_.render_ghost();

  // side and bottom walls (rendered last to cover legacy furniture in
  // illegal positions, e.g. one of the walls is placed into the wall in one of
  // the on disk restaurant layouts)
  render_side_and_bottom_walls();
  walk_scene_.render();
  attack_.render();

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

  // Render the customer entrance door. During an attack we leave the doorway as
  // bare background tiles (no door sprite) so the attacker can walk through it.
  if (!kitchen_view_ && entrance_offset >= 0 &&
      view_mode_ != RestaurantViewMode::OPPONENT_ATTACK) {
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
          if (!in_player_mode()) {
            return;
          }
          // Don't toggle kitchen view if holding furniture - let catchall place
          // it instead
          if (placement_.holding()) {
            return;
          }
          toggle_kitchen_view();
        });
  }
}

void RestaurantView::zoom_out_of_restaurant_view() {
  // go back to CityView
  if (state_.active_city_id().value == -1) {
    state_.active_city(core::CityId{0}); // debug fallback
  }
  state_.ui().push<CityViewState>(state_.active_city_id());
}

void RestaurantView::update_status_display(const PlacedFurniture* pf) {
  auto hub = state_.status();
  StatusDisplayRecipe recipe;
  if (!pf) {
    recipe.add_single("nothing");
  }
  else if (kitchen_view_ &&
           pf->furniture()->role() != FurnitureRole::AshesSmall &&
           pf->furniture()->role() != FurnitureRole::AshesLarge) {
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
  };
  hub.post_direct(msg);
}

void RestaurantView::render_item_picker(
    const RenderContext& ctx, std::string_view title,
    std::span<const core::item::Item> items,
    const std::function<void(core::item::Item)>& on_pick) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back14"));

  const int text_color = 240;
  const ViewPoint title_pos{148, 134};
  context_.font->render_text_onto(*ctx.surface, title, title_pos,
                                  FontFace::PIZ_NUMB, text_color);

  auto* player = state_.active_player();
  auto& inventory = player->inventory();

  int y_position = 164;
  for (auto item : items) {
    auto bag_count = inventory.count(item);

    // Create hover box at current y_position (relative to text_box)
    ViewRect hover_box{122, y_position, 128, 18};

    // Draw background on hover
    if (hovered_item_ && hovered_item_ == item) {
      ctx.surface->fill_rect(hover_box, {15});
    }

    auto name = to_string(item);
    // Text positioned 1px down for vertical centering (1px + text + 1px)
    int text_y = y_position + 1;
    context_.font->render_text_onto(*ctx.surface, name, {124, text_y},
                                    FontFace::PIZ_NUMB, text_color);
    context_.font->render_text_onto(*ctx.surface, std::to_string(bag_count),
                                    {292, text_y}, FontFace::PIZ_NUMB,
                                    text_color, TextAnchor::End);

    interactions_.create_frame_item(
        hover_box, name,
        [this, item, bag_count, on_pick](views::ui::InteractionEvent ev) {
          if (ev.type == views::ui::InteractionEvent::Type::Enter) {
            hovered_item_ = item;
            mark_dirty();
          }
          else if (ev.type == views::ui::InteractionEvent::Type::Leave) {
            hovered_item_ = std::nullopt;
            mark_dirty();
          }
          else if (ev.type == views::ui::InteractionEvent::Type::Click) {
            if (ev.button == views::ui::MouseButton::Left && bag_count > 0) {
              on_pick(item);
            }
          }
        });

    y_position += 20;
  }
}

void RestaurantView::render_sabotage(const RenderContext& ctx) {
  render_item_picker(
      ctx, "which action?", core::item::sabotage_list,
      [this](core::item::Item item) { execute_sabotage_attack(item); });
}

void RestaurantView::render_weapon_select(const RenderContext& ctx) {
  render_item_picker(
      ctx, "which weapon?", core::item::attack_weapon_list,
      [this](core::item::Item item) { attack_.begin_attack(item); });
}

void RestaurantView::execute_sabotage_attack(core::item::Item item) {
  auto* attacker = state_.active_player();
  if (!attacker || !restaurant_) {
    return;
  }

  attacker->use_weapon(state_.time().current_date(), item);
  restaurant_->apply_sabotage(item);

  constexpr int first_sabotage_item =
      static_cast<int>(core::item::Item::SmellyCheese);
  int item_index = static_cast<int>(item) - first_sabotage_item;
  state_.adjust_underworld_score(*attacker, (item_index + 1) * 5);

  auto owner_id = restaurant_->property().owner_or_tenant();
  if (owner_id.has_value()) {
    auto* victim = state_.player(*owner_id);
    if (victim && victim->is_human_controlled()) {
      auto msg_line = static_cast<core::item::ActionLine>(
          static_cast<int>(
              core::item::ActionLine::SmellyCheesePizzasSmellAwful) +
          item_index);
      victim->queue_notification(StatusMsg{
          .payload = compat::format("{}: {}", restaurant_->name(),
                                    core::item::ActionText::get(msg_line)),
          .scope = StatusScope::Restaurant,
      });
    }
  }

  view_mode_ = RestaurantViewMode::OPPONENT_VIEW;
  set_up_buttons();
  mark_dirty();
}

void RestaurantView::challenge_check_and_start(core::PlayerId opp_pid) {
  auto* opp_player = state_.player(opp_pid);
  if (opp_player && !opp_player->is_human_controlled() &&
      opp_player->competition_willingness() != 0) {
    context_.popup->create<AlertPopup>(
        "No thanks! I don't want to participate!");
    return;
  }
  auto* human = state_.active_player();
  if (!human || human->cash_balance() < 500_usd) {
    return;
  }
  auto cash_dollars = static_cast<int>(human->cash_balance().whole());
  auto max_bet = std::min(10000, (cash_dollars / 500) * 500);
  assert(max_bet >= 500);
  CompetitionFlow flow{
      .human_pid = human->id(),
      .opp_pid = opp_pid,
      .max_bet = max_bet,
  };
  challenge_ask_confirm(flow);
}

void RestaurantView::challenge_ask_confirm(CompetitionFlow flow) {
  auto* opp = state_.player(flow.opp_pid);
  const RawRenderable* portrait = opp ? &opp->character() : nullptr;
  context_.popup->origin({28, 38});
  context_.popup->create<ConfirmPopup>(
      "So you want to challenge me to a pizza competition?",
      [this, flow](bool agree) {
        if (agree) {
          challenge_ask_bet(flow);
        }
      },
      portrait);
}

void RestaurantView::challenge_ask_bet(CompetitionFlow flow) {
  context_.popup->create<NumberInputPopup>(
      "How much do you stake?", "$", 500, 500, flow.max_bet, 500,
      [this, flow](int bet_dollars) mutable {
        flow.bet = Money::from_dollars(bet_dollars);
        challenge_ask_speed(flow);
      });
}

void RestaurantView::challenge_ask_speed(CompetitionFlow flow) {
  context_.popup->create<NumberInputPopup>(
      "Speed:", "", 3, 1, 5, 1, [this, flow](int speed_level) mutable {
        flow.speed_level = speed_level;
        challenge_start(flow);
      });
}

void RestaurantView::challenge_start(CompetitionFlow flow) {
  int comp_count = state_.competitions().competition_count();
  // early game: lower IDs (starting at 20) then increasing with comp_count
  int pizza_index =
      std::min(comp_count, 40) + engine::random::rand_max_n(10) + 20;

  auto& zutaten_bob = context_.bob->get("bob/zutaten.bob");
  auto get_bob_size =
      make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob);
  auto reference = PizzaFactory::create_competition_pizza(
      pizza_index, get_bob_size, state_.customer_taste_profiles());
  if (!reference) {
    Logger::warn("Challenge: failed to load pizza {}", pizza_index);
    return;
  }

  auto session = std::make_shared<CompetitionSession>();
  session->pizza_index = pizza_index;
  session->speed_level = flow.speed_level;
  session->delay_ticks = 100 - flow.speed_level * 15;
  session->is_challenge = true;
  session->reference_pizza =
      std::make_shared<core::food::Pizza>(std::move(*reference));
  session->human_slot = static_cast<int>(session->participants.size());
  session->challenger_slot = flow.human_pid.value;
  session->participants.push_back(CompetitionParticipant{
      .player_id = flow.human_pid,
      .is_human = true,
      .bet = flow.bet,
  });
  session->participants.push_back(CompetitionParticipant{
      .player_id = flow.opp_pid,
      .is_human = false,
      .bet = flow.bet,
  });

  state_.competitions().collect_competition_bets(*session);

  state_.ui().push<PizzaDesignState>(PizzaDesignMode::CompetitorChallenge,
                                     std::nullopt, nullptr, std::nullopt,
                                     session);
}

const NavGrid& RestaurantView::navigation_grid() const {
  if (!nav_grid_cache_) {
    // Rasterize the restaurant's furniture into obstacle rects.
    const auto& furniture = restaurant_->layout()->placed_furniture();
    std::vector<WorldRect> obstacles;
    obstacles.reserve(furniture.size());
    for (const auto& pf : furniture) {
      if (!pf) {
        continue;
      }
      const Size sz = bob_renderer_for(*pf).size(*pf);
      obstacles.push_back(
          WorldRect{pf->origin().x, pf->origin().y, sz.w, sz.h});
    }
    nav_grid_cache_ = build_navigation_grid(
        restaurant_->size(), restaurant_->layout()->entrance_offset(),
        obstacles);
  }
  return *nav_grid_cache_;
}
