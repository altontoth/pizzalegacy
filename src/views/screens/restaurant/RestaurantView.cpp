/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/restaurant/RestaurantView.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include "core/Demographics.h"
#include "core/GameTypes.h"
#include "core/character/Player.h"
#include "core/city/District.h"
#include "core/city/Property.h"
#include "core/finance/Ledger.h"
#include "core/item/ActionText.h"
#include "core/restaurant/Furniture.h"
#include "core/restaurant/RestaurantTypes.h"
#include "core/state/CompetitionSession.h"
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
#include "factories/PizzaFactory.h"
#include "gfx/Blit.h"
#include "gfx/Draw.h"
#include "resources/BobSizeFn.h"
#include "views/base/BobRenderer.h"
#include "views/base/ViewDefs.h"
#include "views/screens/restaurant/RestaurantStats.h"
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

// Walker advances one cell every WALKER_MOVE_INTERVAL render frames
static constexpr int WALKER_MOVE_INTERVAL = 4;

// Maps the 4-phase walk cycle onto the 3 sprite frames per direction:
// neutral, left-step, neutral, right-step.
static constexpr std::array<int, 4> WALK_CYCLE_TO_FRAME = {0, 1, 0, 2};

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
      camctrl_{context.input, camera_},
      furniture_placement_(state_.restaurant_placement()),
      return_to_supplier_type_(r_state.return_to_supplier_type),
      status_controller_(state.status()),
      listener_{context_.input->register_listener(*this)} {
  assert(restaurant_);

  // Set this restaurant as the active one so other views know which restaurant
  // we're working with
  state_.active_restaurant(r_state.id);
  if (furniture_placement_.empty()) {
    state_.prevent_auto_end_of_turn(false);
    state_.resume_time_advancement();
  }
  else {
    state_.prevent_auto_end_of_turn(true);
    // only advance explicitly per item placed
    state_.pause_time_advancement();
  }

  // Check if we're placing ovens. If so, start in kitchen view.
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
        if (on_cursor_) {
          on_cursor_->rotate();
          if (show_furniture_cursor_) {
            context_.cursor->custom_cursor(
                bob_renderer_for(*on_cursor_).image(*on_cursor_), false);
          }
          mark_dirty();
        }
        else {
          zoom_out_of_restaurant_view();
        }
      },
      [this](bool lock) { context_.cursor->set_relative_mouse_mode(lock); });
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

RestaurantView::~RestaurantView() {
  state_.prevent_auto_end_of_turn(true);
  attack_.finish_scene();
  context_.audio->stop_music();
  if (current_restaurant_noise_) {
    context_.audio->stop_sound(*current_restaurant_noise_);
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
  const auto owner_pid = restaurant_->property().owner_or_tenant();
  if (!owner_pid) {
    fatal("My current restaurant is not owned by me?!");
  }

  auto push_view = [&](core::RestaurantId rid) {
    // Update the stored city view scroll to center on the new restaurant
    auto* rest = state_.restaurant(rid);
    if (rest) {
      auto cvs = state_.city_view_view_state();
      cvs.center_on_index(rest->property().index());
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
    if (in_player_mode()) {
      toggle_kitchen_view();
      mark_dirty();
    }
    return true;
  }
  case input::Action::Fire: {
    if (attack_.active()) {
      attack_.fire_attacker_weapon();
      return true;
    }
    break;
  }
  case input::Action::P2_Fire: {
    if (attack_.active() && attack_.has_defender()) {
      attack_.fire_defender_weapon();
      return true;
    }
    break;
  }
#ifndef NDEBUG
  case input::Action::Backspace: {
    show_walk_debug_ = !show_walk_debug_;
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
        state_.ui().push<PizzaDesignState>(PizzaDesignMode::Recipes);
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
          if (on_cursor_ && !state_.options().qol_improvements) {
            on_cursor_->rotate();
            // Only update cursor if it's visible (mouse is in viewport)
            if (show_furniture_cursor_) {
              context_.cursor->custom_cursor(
                  bob_renderer_for(*on_cursor_).image(*on_cursor_), false);
            }
            mark_dirty();
          }
          else if (!on_cursor_ && !state_.options().qol_improvements) {
            zoom_out_of_restaurant_view();
          }
          return;
        }
        // Left click handling
        if (view_mode_ == RestaurantViewMode::OPPONENT_VIEW ||
            view_mode_ == RestaurantViewMode::OPPONENT_ATTACK) {
          return;
        }
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
          // Suppress ghost updates while RMB scrolling to prevent the ghost
          // from randomly appearing
          if (rmb_drag_scroller_.is_active()) {
            break;
          }
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
                  bob_renderer_for(*on_cursor_).image(*on_cursor_), false);
              show_furniture_cursor_ = true;
              state_.pause_time_advancement();
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

        case views::ui::InteractionEvent::Type::Scroll:
          // Item cycling via scroll wheel lives here rather than in the
          // catchall because furniture frame items steal the normal-layer
          // scroll event when the mouse is over placed furniture, and they
          // ignore scroll entirely. Background items receive scroll
          // regardless, so cycling works everywhere in the viewport.
          if (return_to_supplier_type_) {
            if (ev.scroll_delta > 0) {
              handle_next_item();
            }
            else if (ev.scroll_delta < 0) {
              handle_previous_item();
            }
          }
          break;

        default:
          break;
        }
      },
      {}, // no tooltip
      views::ui::InteractionLayer::Background);

  rmb_drag_scroller_.add_interaction(interactions_, vp_local,
                                     [this] { mark_dirty(); });
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

    const bool placing_table =
        on_cursor_->furniture()->category() == FurnitureCategory::TABLE;
    const core::FurnitureId new_table_id = on_cursor_->id();
    const WorldRect table_cand = cand;

    // cand already has the correct top-left position (calculated by
    // candidate_rect) Pass the top-left corner for placement
    place_cursor_item(cand.point());

    // When placing a table, sync chair links: link overlapping chairs and
    // unlink no longer overlapping chairs
    if (placing_table) {
      for (const auto& pf : current_furniture_list()) {
        if (pf->furniture()->category() != FurnitureCategory::CHAIR) {
          continue;
        }
        const Size chair_size = bob_renderer_for(*pf).size(*pf);
        const WorldRect chair_rect{pf->origin(), chair_size};
        const bool overlaps = chair_rect.intersects(table_cand) &&
                              !table_cand.contains(chair_rect.center());
        if (overlaps) {
          pf->linked_furniture_id(new_table_id);
        }
        else if (pf->linked_furniture_id() == new_table_id) {
          pf->unlink_furniture();
        }
      }
    }
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
  if (attack_.active()) {
    // In attack mode by default the arrow keys move the attacker, not the
    // camera, so we have to skip camctrl_ here since it's bound to the same
    // keys. The attack scene moves the camera when they get close to the edge
    // so regardless of key bindings we just return after ticking the attack.
    attack_.tick();
    return;
  }

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
  if (walking_guest_) {
    walking_tick();
  }
  else if (restaurant_->is_open(state_.time().time_of_day()) &&
           !kitchen_view_) {
    // Only roll once per animation frame (every WALKER_MOVE_INTERVAL renders)
    // so the effective spawn rate matches the original
    ++spawn_anim_gate_;
    if (spawn_anim_gate_ < WALKER_MOVE_INTERVAL) {
      return;
    }
    spawn_anim_gate_ = 0;
    // Two independent 10% rolls each tick -> ~1% spawn chance per tick.
    const bool roll1 = engine::random::rand_percentage_chance(10);
    const bool roll2 = engine::random::rand_percentage_chance(10);
    if (roll1 && roll2) {
      const int mode = engine::random::rand_max_n(3) + 1;

      if (mode == 1) {
        spawn_guest();
      }
      else if (mode == 2) {
        spawn_guest_departure();
      }
      else {
        spawn_waiter();
      }
    }
  }
}

void RestaurantView::handle_furniture_click(const PlacedFurniture* pf) {
  const auto prop_id = pf->id();
  if ((kitchen_view_ || !restaurant_->is_open(state_.time().time_of_day())) &&
      last_clicked_furniture_index_ &&
      *last_clicked_furniture_index_ == prop_id) {
    auto picked = restaurant_->layout()->remove_by_id(prop_id, kitchen_view_);
    assert(picked && "remove_by_id failed");
    assert(picked->id() == prop_id && "picked id changed unexpectedly");
    if (!kitchen_view_) {
      invalidate_nav_grid();
    }

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
      context_.popup->create<JukeboxPopup>(restaurant_);
    }
    else if (ftype == FurnitureType::TV_AERIAL ||
             ftype == FurnitureType::TV_CABLE ||
             ftype == FurnitureType::TV_SATELLITE) {
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
  for (const auto& up : current_furniture_list()) {
    const auto* of = up->furniture();
    if (of->category() != FurnitureCategory::DECOR ||
        of->placement_layer() != PlacementLayer::Top) {
      continue;
    }
    const WorldRect other_map = rect_of(*up, bob_renderer_for(*up));
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
      const auto category = pf->furniture()->category();
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
                  const Size decor_sz =
                      bob_renderer_for(*on_cursor_).size(*on_cursor_);
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
              if (on_cursor_ && !state_.options().qol_improvements) {
                on_cursor_->rotate();
                // Only update cursor if it's visible (mouse is in viewport)
                if (show_furniture_cursor_) {
                  context_.cursor->custom_cursor(
                      bob_renderer_for(*on_cursor_).image(*on_cursor_), false);
                }
                mark_dirty();
              }
              else if (!on_cursor_ && !state_.options().qol_improvements) {
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

namespace {

ConsumerType pick_weighted_demographic(const CustomerCounts& counts) {
  int total = 0;
  for (auto ct : ALL_CONSUMER_TYPES) {
    total += counts[ct];
  }
  if (total == 0) {
    constexpr int DEMO_COUNT = static_cast<int>(ConsumerType::Count);
    return static_cast<ConsumerType>(engine::random::rand_max_n(DEMO_COUNT));
  }
  int roll = engine::random::rand_max_n(total);
  for (ConsumerType ct : ALL_CONSUMER_TYPES) {
    roll -= counts[ct];
    if (roll < 0) {
      return ct;
    }
  }
  return ALL_CONSUMER_TYPES.back();
}

int pick_guest_type(ConsumerType demo) {
  return static_cast<int>(demo) + (engine::random::coin_flip() ? 6 : 0);
}

int pick_face_index(int guest_type) {
  return (guest_type * 2) + (engine::random::coin_flip() ? 1 : 0);
}

} // namespace

void RestaurantView::rebuild_guest_cache() {
  guests_cache_.clear();

  // Only show guests in restaurant view (not kitchen) and when open
  if (kitchen_view_ || !restaurant_->is_open(state_.time().time_of_day())) {
    shuffled_chairs_.clear();
    guest_occupancy_delta_ = 0;
    return;
  }

  const auto [guests_to_show, seat_count] = restaurant_->guest_occupancy();
  int effective_guests =
      std::min(guests_to_show + guest_occupancy_delta_, seat_count);
  if (effective_guests <= 0) {
    return;
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
    return;
  }

  // Exclude a chair whose guest is currently walking out
  if (departing_chair_id_) {
    for (int i = 0; i < effective_guests && i < (int)shuffled_chairs_.size();
         ++i) {
      if (shuffled_chairs_[i]->id() == *departing_chair_id_) {
        --effective_guests;
        break;
      }
    }
    if (effective_guests <= 0) {
      return;
    }
  }

  const auto& customer_counts = restaurant_->economy().customer_counts();

  // Fill seats from the shuffled chair list.

  using namespace views::screens::restaurant;

  constexpr std::array<int, 4> seating_offset_x = {-4, -3, -3, 4};
  constexpr std::array<int, 4> seating_offset_y = {2, -5, -6, -9};

  int chair_it = 0; // raw index into shuffled_chairs_
  for (int placed = 0;
       placed < effective_guests && chair_it < (int)shuffled_chairs_.size();) {
    const PlacedFurniture* pf = shuffled_chairs_[chair_it++];
    // Skip the chair whose guest is currently walking out; it is drawn by the
    // walker system rather than as a seated guest, and effective_guests was
    // already decremented above to leave its slot empty.
    if (departing_chair_id_ && pf->id() == *departing_chair_id_) {
      continue;
    }
    ++placed;
    const core::FurnitureId chair_id = pf->id();

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
    //
    // Create a controller lazily and keep it for the duration of the seating.
    // Walker arrivals already have a controller, so they fall straight through
    // to rendering; only chairs we seat directly are filled in here.
    auto it = guest_controllers_.find(chair_id);
    if (it == guest_controllers_.end()) {
      const ConsumerType demo = pick_weighted_demographic(customer_counts);
      CharacterController controller;
      controller.set_customer_type(demo);
      controller.set_eating(false); // waiter delivers pizza to start eating
      const int gt = pick_guest_type(demo);
      controller.set_guest_type(gt);
      controller.set_face_index(pick_face_index(gt));
      it = guest_controllers_.emplace(chair_id, controller).first;
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
    guests_cache_.push_back(GuestRenderItem{
        .torso_anchor_map = guest_pos,
        .pose = pose,
        .chair_id = chair_id,
        .interacted_before = false,
        .face_index = it->second.face_index(),
    });
  }
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
  const auto size = bob_renderer_for(ghost).size(ghost);
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
    const auto other_size = bob_renderer_for(*pf).size(*pf);
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

int RestaurantView::walk_frame_for(WalkingGuest::Orientation dir,
                                   int walk_cycle) {
  return (static_cast<int>(dir) * 3) + WALK_CYCLE_TO_FRAME[walk_cycle];
}

int RestaurantView::walk_frame_for(const WalkingGuest& g) {
  return walk_frame_for(g.direction, g.walk_cycle);
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
    invalidate_nav_grid();
  }

  state_.advance_time_blocks(1);
  state_.resume_time_advancement();

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

  if (view_mode_ == RestaurantViewMode::WEAPON_SELECT) {
    render_weapon_select(ctx);
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
#ifndef NDEBUG
  if (show_walk_debug_ && dbg_walk_mode_ == 3 && !kitchen_view_) {
    render_debug_full_grid();
  }
#endif
  render_walking_guest();
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
          if (on_cursor_) {
            return;
          }
          toggle_kitchen_view();
        });
  }
}

#ifndef NDEBUG
void RestaurantView::render_debug_full_grid() {
  static constexpr gfx::ColorIndex FREE_COLOR = 80;
  static constexpr gfx::ColorIndex BLOCKED_COLOR = 68;
  static constexpr int C = WALK_CELL_SIZE;

  const auto& [blocked, grid_w, grid_h] = navigation_grid();

  for (int gy = 0; gy < grid_h; ++gy) {
    for (int gx = 0; gx < grid_w; ++gx) {
      const gfx::ColorIndex col =
          blocked[(gy * grid_w) + gx] ? BLOCKED_COLOR : FREE_COLOR;
      gfx::draw::box(background_, ViewPoint{gx * C, gy * C},
                     ViewPoint{(gx * C) + C - 1, (gy * C) + C - 1}, col, 1);
    }
  }
}
#endif

WorldPoint
RestaurantView::clamp_furniture_to_interior(WorldPoint mouse_world) const {
  const auto sz = bob_renderer_for(*on_cursor_).size(*on_cursor_);
  const auto room_size =
      kitchen_view_ ? restaurant_->kitchen_size() : restaurant_->size();

  constexpr int tile_w = RESTAURANT_TILE_WIDTH;
  constexpr int tile_h = RESTAURANT_TILE_HEIGHT;

  // Interior bounds in world coordinates
  // Furniture top-left must be at least at (tile_w, tile_h) and at most
  // so that bottom-right doesn't exceed the interior
  const int interior_min_x = tile_w;
  const int interior_min_y = tile_h;
  const int interior_max_x = ((room_size.w + 1) * tile_w) - sz.w;
  const int interior_max_y = ((room_size.h + 1) * tile_h) - sz.h;

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
  // and not mid-scroll-drag (ghost position is stale while camera moves).
  if (!on_cursor_ || !ghost_furniture_topleft_ || show_furniture_cursor_ ||
      rmb_drag_scroller_.is_active()) {
    return;
  }

  const auto& bob_surface = bob_renderer_for(*on_cursor_).image(*on_cursor_);

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
                             [this]() { handle_throw_away(); });
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
    FurnitureBatch move_batch{.furniture = batch->furniture,
                              .count = 1,
                              .bob_state = batch->bob_state};
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
        .reopen_restaurant = closed_for_placement_,
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
      refresh_furniture_cursor_image();
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
      refresh_furniture_cursor_image();
      mark_dirty();
      return;
    }
  }
}

void RestaurantView::refresh_furniture_cursor_image() {
  if (on_cursor_ && show_furniture_cursor_) {
    context_.cursor->custom_cursor(
        bob_renderer_for(*on_cursor_).image(*on_cursor_), false);
  }
}

void RestaurantView::update_status_display(const PlacedFurniture* pf) {
  auto hub = state_.status();
  StatusDisplayRecipe recipe;
  if (!pf) {
    recipe.add_single("nothing");
  }
  else if (kitchen_view_ &&
           pf->furniture()->type() != FurnitureType::ASHES_SMALL &&
           pf->furniture()->type() != FurnitureType::ASHES_LARGE) {
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
                                    text_color, AlignX::RightEdge);

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

  attacker->inventory().remove(item, 1);
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
        flow.bet_dollars = bet_dollars;
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
  int comp_count = state_.competition_count();
  // early game: lower IDs (starting at 20) then increasing with comp_count
  int pizza_index =
      std::min(comp_count, 40) + engine::random::rand_max_n(10) + 20;

  auto& zutaten_bob = context_.bob->get("bob/zutaten.bob");
  auto get_bob_size =
      make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob);
  auto canonical = PizzaFactory::create_pizza(pizza_index, get_bob_size,
                                              state_.customer_taste_profiles());
  if (!canonical) {
    Logger::warn("Challenge: failed to load pizza {}", pizza_index);
    return;
  }

  Money bet{static_cast<int64_t>(flow.bet_dollars) * 100};
  auto session = std::make_shared<CompetitionSession>();
  session->pizza_index = pizza_index;
  session->speed_level = flow.speed_level;
  session->delay_ticks = 100 - flow.speed_level * 15;
  session->is_challenge = true;
  session->canonical_pizza =
      std::make_shared<core::food::Pizza>(std::move(*canonical));
  session->human_slot = static_cast<int>(session->participants.size());
  session->challenger_slot = flow.human_pid.value;
  session->participants.push_back(CompetitionParticipant{
      .player_id = flow.human_pid,
      .is_human = true,
      .bet = bet,
  });
  session->participants.push_back(CompetitionParticipant{
      .player_id = flow.opp_pid,
      .is_human = false,
      .bet = bet,
  });

  state_.ui().push<PizzaDesignState>(PizzaDesignMode::CompetitorChallenge,
                                     std::nullopt, nullptr, std::nullopt,
                                     session);
}

void RestaurantView::record_walk_debug_destination(WorldPoint destination) {
#ifndef NDEBUG
  dbg_walk_destination_ = destination;
#else
  (void)destination;
#endif
}

void RestaurantView::snapshot_walk_debug_grid(WalkingGuest& walker,
                                              WalkingPath& wp) {
#ifndef NDEBUG
  walker.dbg_blocked = std::move(wp.blocked);
  walker.dbg_grid_w = wp.grid_w;
  walker.dbg_grid_h = wp.grid_h;
#else
  (void)walker;
  (void)wp;
#endif
}

void RestaurantView::render_walking_guest() {
  if (!walking_guest_ || kitchen_view_) {
    return;
  }
  const auto& walker = walking_guest_.value();

  if (!camera_.visible_world_rect().contains(walker.position)) {
    return;
  }

#ifndef NDEBUG
  if (show_walk_debug_) {
    static constexpr gfx::ColorIndex BLOCKED_COLOR = 71;
    static constexpr gfx::ColorIndex ANCHOR_COLOR = 15;
    static constexpr gfx::ColorIndex DEST_COLOR = 9;
    static constexpr gfx::ColorIndex AHEAD_COLOR = 104;
    static constexpr gfx::ColorIndex PAST_COLOR = 72;
    static constexpr int C = WALK_CELL_SIZE;
    static constexpr int H = C / 2;

    // mode 3 = full nav grid (drawn before this) + path overlay on top
    const bool show_grid = (dbg_walk_mode_ == 0 || dbg_walk_mode_ == 1);
    const bool show_path = (dbg_walk_mode_ != 1);
    constexpr int line_width = 1;

    // Blocked cells
    if (show_grid) {
      for (int gy = 0; gy < walker.dbg_grid_h; ++gy) {
        for (int gx = 0; gx < walker.dbg_grid_w; ++gx) {
          if (walker.dbg_blocked[(gy * walker.dbg_grid_w) + gx]) {
            gfx::draw::box(background_, ViewPoint{gx * C, gy * C},
                           ViewPoint{(gx * C) + C - 1, (gy * C) + C - 1},
                           BLOCKED_COLOR, line_width);
          }
        }
      }
    }

    if (show_path && !walker.path.empty()) {
      // Get starting cell center by rewinding path_index steps.
      int px = ((walker.position.x / C) * C) + H;
      int py = ((walker.position.y / C) * C) + H;
      for (int i = walker.path_index - 1; i >= 0; --i) {
        const WalkingGuest::Orientation d = walker.path[i];
        px -= walk_dx(d) * C;
        py -= walk_dy(d) * C;
      }

      for (int i = 0; i < static_cast<int>(walker.path.size()); ++i) {
        const gfx::ColorIndex col =
            (i >= walker.path_index) ? AHEAD_COLOR : PAST_COLOR;
        const WalkingGuest::Orientation d = walker.path[i];
        const int nx = px + (walk_dx(d) * C);
        const int ny = py + (walk_dy(d) * C);

        gfx::draw::box(background_, ViewPoint{px - H, py - H},
                       ViewPoint{px + H - 1, py + H - 1}, col, line_width);

        // Direction arrow:
        const int perp_x = -walk_dy(d);
        const int perp_y = walk_dx(d);
        const ViewPoint tip{px + (walk_dx(d) * 5), py + (walk_dy(d) * 5)};
        const int bx = px - (walk_dx(d) * 2);
        const int by = py - (walk_dy(d) * 2);
        gfx::draw::line(background_,
                        ViewPoint{bx + (perp_x * 3), by + (perp_y * 3)}, tip,
                        col, line_width);
        gfx::draw::line(background_,
                        ViewPoint{bx - (perp_x * 3), by - (perp_y * 3)}, tip,
                        col, line_width);

        gfx::draw::line(background_, ViewPoint{px, py}, ViewPoint{nx, ny}, col,
                        line_width);

        px = nx;
        py = ny;
      }

      // Final cell outline (path end)
      gfx::draw::box(background_, ViewPoint{px - H, py - H},
                     ViewPoint{px + H - 1, py + H - 1}, AHEAD_COLOR,
                     line_width);

      // Walker anchor crosshair
      const int ax = walker.position.x;
      const int ay = walker.position.y;
      gfx::draw::line(background_, ViewPoint{ax - 5, ay}, ViewPoint{ax + 5, ay},
                      ANCHOR_COLOR, line_width);
      gfx::draw::line(background_, ViewPoint{ax, ay - 5}, ViewPoint{ax, ay + 5},
                      ANCHOR_COLOR, line_width);
    }

    // Draw an X for the destination
    if (dbg_walk_destination_) {
      const int dx = dbg_walk_destination_->x;
      const int dy = dbg_walk_destination_->y;
      gfx::draw::line(background_, ViewPoint{dx - 5, dy - 5},
                      ViewPoint{dx + 5, dy + 5}, DEST_COLOR, line_width);
      gfx::draw::line(background_, ViewPoint{dx + 5, dy - 5},
                      ViewPoint{dx - 5, dy + 5}, DEST_COLOR, line_width);
    }
  }
#endif
  const auto& guest_bob =
      context_.bob->get(compat::format("bob/walk{}.bob", walker.guest_type));
  const ViewPoint blit_pos{walker.position.x - walker.sprite_blit_offset_x,
                           walker.position.y - walker.sprite_blit_offset_y};
  background_.blit_masked(guest_bob.frame(walk_frame_for(walker)), blit_pos);

  if (walker.kind == WalkingGuest::WalkerKind::WAITER_DELIVERING ||
      walker.kind == WalkingGuest::WalkerKind::WAITER_RETURNING_WITH_PIZZA) {
    // In order of WalkingGuest::Orientation
    static constexpr std::array<std::array<int, 3>, ORIENTATION_COUNT>
        pizza_dx = {{
            {2, 1, 2},
            {5, 6, 1},
            {18, 17, 15},
            {15, 24, 18},
        }};
    static constexpr std::array<std::array<int, 3>, ORIENTATION_COUNT>
        pizza_dy = {{
            {6, 9, 3},
            {0, 0, 0},
            {-6, -7, -13},
            {7, 8, 9},
        }};

    const int dir = static_cast<int>(walker.direction);
    const int frm = WALK_CYCLE_TO_FRAME[walker.walk_cycle];
    const int x_off = pizza_dx[dir][frm];
    const int y_off = pizza_dy[dir][frm];

    const Surface pizza = pizza_bob_.frame(340);
    background_.blit_masked(pizza, ViewPoint{
                                       blit_pos.x + (x_off * 2),
                                       blit_pos.y + ((y_off + 8) * 2),
                                   });
  }
}

namespace {

constexpr int WALK_MAX_OPS = 300;
constexpr int WALK_MAX_DEPTH = 100;

bool walk_dfs(int cx, int cy, int dest_x, int dest_y, int grid_w, int grid_h,
              const std::vector<bool>& blocked, std::vector<bool>& visited,
              std::vector<WalkingGuest::Orientation>& path, int& ops_remaining,
              int depth) {
  if (cx == dest_x && cy == dest_y) {
    return true;
  }
  if (ops_remaining <= 0 || depth >= WALK_MAX_DEPTH) {
    return false;
  }
  --ops_remaining;
  visited[(cy * grid_w) + cx] = true;

  using O = WalkingGuest::Orientation;

  // Score each direction by the signed displacement toward the destination
  // along that axis. Higher orientation value wins ties
  auto dir_score = [&](O dir) -> int {
    switch (dir) {
    case O::S:
      return dest_y - cy;
    case O::W:
      return cx - dest_x;
    case O::N:
      return cy - dest_y;
    case O::E:
      return dest_x - cx;
    }
    return 0;
  };

  std::array<O, ORIENTATION_COUNT> dirs = {O::S, O::W, O::N, O::E};
  std::ranges::sort(dirs, [&](O a, O b) {
    const int sa = dir_score(a);
    const int sb = dir_score(b);
    if (sa != sb) {
      return sa > sb;
    }
    return a > b;
  });

  for (O dir : dirs) {
    const int nx = cx + walk_dx(dir);
    const int ny = cy + walk_dy(dir);
    if (nx < 0 || nx >= grid_w || ny < 0 || ny >= grid_h) {
      continue;
    }
    const int idx = (ny * grid_w) + nx;
    if (blocked[idx] || visited[idx]) {
      continue;
    }
    path.push_back(dir);
    if (walk_dfs(nx, ny, dest_x, dest_y, grid_w, grid_h, blocked, visited, path,
                 ops_remaining, depth + 1)) {
      return true;
    }
    path.pop_back();
  }
  return false;
}

// Collapse runs into L-shapes; the DFS produces staircases that look bad.
bool seg_clear(int x0, int y0, int x1, int y1, int grid_w, int grid_h,
               const std::vector<bool>& blocked) {
  const int dx = (x1 > x0) ? 1 : (x1 < x0) ? -1 : 0;
  const int dy = (y1 > y0) ? 1 : (y1 < y0) ? -1 : 0;
  int x = x0;
  int y = y0;
  while (x != x1 || y != y1) {
    if (x < 0 || x >= grid_w || y < 0 || y >= grid_h ||
        blocked[(y * grid_w) + x]) {
      return false;
    }
    x += dx;
    y += dy;
  }
  return x >= 0 && x < grid_w && y >= 0 && y < grid_h &&
         !blocked[(y * grid_w) + x];
}

std::vector<WalkingGuest::Orientation>
straighten_path(int from_x, int from_y,
                const std::vector<WalkingGuest::Orientation>& path, int grid_w,
                int grid_h, const std::vector<bool>& blocked) {
  using O = WalkingGuest::Orientation;

  if (path.size() <= 1) {
    return path;
  }

  std::vector<std::pair<int, int>> pos;
  pos.reserve(path.size() + 1);
  pos.emplace_back(from_x, from_y);
  for (auto d : path) {
    pos.emplace_back(pos.back().first + walk_dx(d),
                     pos.back().second + walk_dy(d));
  }

  std::vector<O> result;
  result.reserve(path.size());
  int from_idx = 0;

  while (from_idx < static_cast<int>(path.size())) {
    const auto [fx, fy] = pos[from_idx];
    bool jumped = false;

    // Scan backwards: find the furthest pos[to] reachable via an L-path.
    for (int to = static_cast<int>(pos.size()) - 1; to > from_idx + 1; --to) {
      const auto [tx, ty] = pos[to];

      if (seg_clear(fx, fy, tx, fy, grid_w, grid_h, blocked) &&
          seg_clear(tx, fy, tx, ty, grid_w, grid_h, blocked)) {
        if (tx != fx) {
          const O h_dir = (tx > fx) ? O::E : O::W;
          for (int k = 0; k < std::abs(tx - fx); ++k) {
            result.push_back(h_dir);
          }
        }
        if (ty != fy) {
          const O v_dir = (ty > fy) ? O::S : O::N;
          for (int k = 0; k < std::abs(ty - fy); ++k) {
            result.push_back(v_dir);
          }
        }
        from_idx = to;
        jumped = true;
        break;
      }

      if (seg_clear(fx, fy, fx, ty, grid_w, grid_h, blocked) &&
          seg_clear(fx, ty, tx, ty, grid_w, grid_h, blocked)) {
        if (ty != fy) {
          const O v_dir = (ty > fy) ? O::S : O::N;
          for (int k = 0; k < std::abs(ty - fy); ++k) {
            result.push_back(v_dir);
          }
        }
        if (tx != fx) {
          const O h_dir = (tx > fx) ? O::E : O::W;
          for (int k = 0; k < std::abs(tx - fx); ++k) {
            result.push_back(h_dir);
          }
        }
        from_idx = to;
        jumped = true;
        break;
      }
    }

    if (!jumped) {
      result.push_back(path[from_idx]);
      ++from_idx;
    }
  }

  return result;
}

} // namespace

void RestaurantView::walking_tick() {
  auto& walker = *walking_guest_;

  // Walkers move one whole cell every WALKER_MOVE_INTERVAL render frames.
  ++walker.anim_tick;
  if (walker.anim_tick < WALKER_MOVE_INTERVAL) {
    return;
  }
  walker.anim_tick = 0;

  if (walker.path_index >= static_cast<int>(walker.path.size())) {
    switch (walker.kind) {
    case WalkingGuest::WalkerKind::GUEST:
      if (walker.walking_out) {
        const core::FurnitureId failed_id = walker.destination_chair_id;
        auto it = std::ranges::find_if(
            shuffled_chairs_,
            [&](const PlacedFurniture* pf) { return pf->id() == failed_id; });
        if (it != shuffled_chairs_.end()) {
          std::rotate(it, it + 1, shuffled_chairs_.end());
        }
        walking_guest_.reset();
      }
      else {
        seat_arrived_guest();
      }
      break;
    case WalkingGuest::WalkerKind::GUEST_DEPARTING:
      departing_chair_id_.reset();
      walking_guest_.reset();
      break;
    case WalkingGuest::WalkerKind::WAITER_DELIVERING:
      waiter_delivered();
      break;
    case WalkingGuest::WalkerKind::WAITER_RETURNING:
    case WalkingGuest::WalkerKind::WAITER_RETURNING_WITH_PIZZA:
      walking_guest_.reset();
      break;
    }
    mark_dirty();
    return;
  }

  walker.direction = walker.path[walker.path_index++];
  step_one_cell(walker.position, walker.walk_cycle, walker.direction);
  mark_dirty();
}

void RestaurantView::seat_arrived_guest() {
  const auto& walker = *walking_guest_;
  views::screens::restaurant::CharacterController ctrl;
  ctrl.set_customer_type(walker.demographic);
  ctrl.set_eating(false);
  ctrl.set_guest_type(walker.guest_type);
  ctrl.set_face_index(walker.face_index);
  guest_controllers_[walker.destination_chair_id] = ctrl;
  ++guest_occupancy_delta_;
  walking_guest_.reset();
  mark_dirty();
}

void RestaurantView::spawn_guest_departure() {
  if (guest_controllers_.empty()) {
    return;
  }

  auto it = guest_controllers_.begin();
  const core::FurnitureId chair_id = it->first;
  const auto& ctrl = it->second;

  const PlacedFurniture* chair =
      restaurant_->layout()->find_by_id(chair_id, false);
  if (!chair) {
    guest_controllers_.erase(it);
    return;
  }

  const Size chair_sz = bob_renderer_for(*chair).size(*chair);
  const WorldPoint from_pos{chair->origin().x + (chair_sz.w / 2),
                            chair->origin().y + (chair_sz.h / 2)};

  constexpr int tile_w = RESTAURANT_TILE_WIDTH;
  constexpr int tile_h = RESTAURANT_TILE_HEIGHT;
  const auto room_size = restaurant_->size();
  const int entrance_offset = restaurant_->layout()->entrance_offset();
  const int logical_x = (entrance_offset * tile_w) + (3 * tile_w / 2);
  const int entry_y = (room_size.h - 1) * tile_h;
  const int entry_cells =
      (background_height_ - entry_y + WALK_CELL_SIZE - 1) / WALK_CELL_SIZE;

  const ConsumerType demographic = ctrl.pose().customer_type;
  const int guest_type = ctrl.guest_type();
  const int face_index = ctrl.face_index();

  const auto& walk_bob =
      context_.bob->get(compat::format("bob/walk{}.bob", guest_type));
  const int blit_offset_x = (walk_bob.width(0) / 2) + 10;
  const int blit_offset_y = (walk_bob.height(0) / 2) + 16;

  const WorldPoint exit_pos{logical_x, entry_y};
  auto wp = compute_walking_path(from_pos, exit_pos, chair->origin(), chair_sz);

  std::vector<WalkingGuest::Orientation> path = std::move(wp.steps);
  path.insert(path.end(), entry_cells, WalkingGuest::Orientation::S);

  // Remove from seated state before the walker starts moving (matches
  // original).
  --guest_occupancy_delta_;
  guest_controllers_.erase(it);

  // Rotate the departing chair to the back so it falls outside the occupied
  // range (0..effective_guests-1) immediately.
  {
    auto sit =
        std::ranges::find_if(shuffled_chairs_, [&](const PlacedFurniture* pf) {
          return pf->id() == chair_id;
        });
    if (sit != shuffled_chairs_.end()) {
      std::rotate(sit, sit + 1, shuffled_chairs_.end());
    }
  }

  departing_chair_id_ = chair_id;

  record_walk_debug_destination(exit_pos);
  walking_guest_ = WalkingGuest{
      .position = from_pos,
      .kind = WalkingGuest::WalkerKind::GUEST_DEPARTING,
      .demographic = demographic,
      .destination = exit_pos,
      .destination_chair_id = chair_id,
      .guest_type = guest_type,
      .face_index = face_index,
      .path = std::move(path),
      .sprite_blit_offset_x = blit_offset_x,
      .sprite_blit_offset_y = blit_offset_y,
  };
  snapshot_walk_debug_grid(*walking_guest_, wp);

  mark_dirty();
}

void RestaurantView::spawn_guest() {
  const PlacedFurniture* chair = get_guest_destination_chair();
  if (!chair) {
    return;
  }

  constexpr int tile_w = RESTAURANT_TILE_WIDTH;
  constexpr int tile_h = RESTAURANT_TILE_HEIGHT;
  const auto room_size = restaurant_->size();
  const int entrance_offset = restaurant_->layout()->entrance_offset();
  const int logical_x = (entrance_offset * tile_w) + (3 * tile_w / 2);
  const int entry_y = (room_size.h - 1) * tile_h;
  const int entry_cells =
      (background_height_ - entry_y + WALK_CELL_SIZE - 1) / WALK_CELL_SIZE;
  const int spawn_y = entry_y + (entry_cells * WALK_CELL_SIZE);

  const ConsumerType demographic =
      pick_weighted_demographic(restaurant_->economy().customer_counts());
  const int guest_type = pick_guest_type(demographic);

  // World pos = sprite_center + (5, 8) (from original, then 2x)
  const auto& walk_bob =
      context_.bob->get(compat::format("bob/walk{}.bob", guest_type));
  const int blit_offset_x = (walk_bob.width(0) / 2) + 10;
  const int blit_offset_y = (walk_bob.height(0) / 2) + 16;

  const Size chair_sz = bob_renderer_for(*chair).size(*chair);
  const WorldPoint destination{chair->origin().x + (chair_sz.w / 2),
                               chair->origin().y + (chair_sz.h / 2)};
  const core::FurnitureId chair_id = chair->id();
  const int face_index = pick_face_index(guest_type);

  std::vector<WalkingGuest::Orientation> path(entry_cells,
                                              WalkingGuest::Orientation::N);

  record_walk_debug_destination(destination);
  auto wp = compute_walking_path(WorldPoint{logical_x, entry_y}, destination,
                                 chair->origin(), chair_sz);

  bool walking_out = false;
  if (wp.steps.empty()) {
    // No path to chair: append exit steps (south) so the guest walks back out.
    path.insert(path.end(), entry_cells, WalkingGuest::Orientation::S);
    walking_out = true;
  }
  else {
    path.insert(path.end(), wp.steps.begin(), wp.steps.end());
  }

  walking_guest_ = WalkingGuest{
      .position = {logical_x, spawn_y},
      .demographic = demographic,
      .destination = destination,
      .destination_chair_id = chair_id,
      .guest_type = guest_type,
      .face_index = face_index,
      .path = std::move(path),
      .sprite_blit_offset_x = blit_offset_x,
      .sprite_blit_offset_y = blit_offset_y,
      .walking_out = walking_out,
  };
  snapshot_walk_debug_grid(*walking_guest_, wp);
}

RestaurantView::NavGrid RestaurantView::build_navigation_grid() const {
  const auto room_size = restaurant_->size();

  // The navigation grid covers the entire rendered restaurant, including the
  // walls. Furniture coordinates already include this border offset, so world
  // coordinates map directly to grid cells.
  const int grid_w =
      ((room_size.w + 2) * RESTAURANT_TILE_WIDTH + WALK_CELL_SIZE - 1) /
      WALK_CELL_SIZE;
  const int grid_h =
      ((room_size.h + 2) * RESTAURANT_TILE_HEIGHT + WALK_CELL_SIZE - 1) /
      WALK_CELL_SIZE;

  std::vector<bool> blocked(static_cast<size_t>(grid_w * grid_h), false);

  // WALK_CELL_SIZE (12) does not divide evenly into the room height measured
  // in pixels (22-pixel tiles). Near the bottom edge, a navigation cell can
  // partially overlap the wall border: treat those cells as blocked if their
  // lower edge extends into the wall, rather than testing only the top left of
  // the cell.
  for (int gy = 0; gy < grid_h; ++gy) {
    for (int gx = 0; gx < grid_w; ++gx) {
      const int wx = gx * WALK_CELL_SIZE;
      const int wy = gy * WALK_CELL_SIZE;
      const bool left_wall = wx < RESTAURANT_TILE_WIDTH;
      const bool right_wall = wx >= (room_size.w + 1) * RESTAURANT_TILE_WIDTH;
      const bool top_wall = wy < RESTAURANT_TILE_HEIGHT;
      const bool bottom_wall = (gy + 1) * WALK_CELL_SIZE >
                               (room_size.h + 1) * RESTAURANT_TILE_HEIGHT;
      if (left_wall || right_wall || top_wall || bottom_wall) {
        blocked[(gy * grid_w) + gx] = true;
      }
    }
  }

  // A cell is blocked only if its center point lies within the furniture
  // bounds. This prevents furniture from claiming neighbouring cells that are
  // touched only by its outer pixels.
  const int half = WALK_CELL_SIZE / 2;
  for (const auto& pf : restaurant_->layout()->placed_furniture()) {
    if (!pf) {
      continue;
    }
    const Size sz = bob_renderer_for(*pf).size(*pf);
    const int ox = pf->origin().x;
    const int oy = pf->origin().y;
    const int x0 = (ox + half - 1) / WALK_CELL_SIZE;
    const int y0 = (oy + half - 1) / WALK_CELL_SIZE;
    const int x1 = (ox + sz.w - half - 1) / WALK_CELL_SIZE;
    const int y1 = (oy + sz.h - half - 1) / WALK_CELL_SIZE;
    for (int y = std::max(0, y0); y <= std::min(grid_h - 1, y1); ++y) {
      for (int x = std::max(0, x0); x <= std::min(grid_w - 1, x1); ++x) {
        blocked[(y * grid_w) + x] = true;
      }
    }
  }

  // Carve the customer entrance through the bottom wall so characters can walk
  // in and out through the doorway. The opening is 3 tiles wide at the entrance
  // offset (matching the brown-tile gap drawn by render_side_and_bottom_walls).
  // Done last so it overrides both the wall and any furniture in the doorway.
  const int entrance_offset = restaurant_->layout()->entrance_offset();
  const int ent_wx0 = entrance_offset * RESTAURANT_TILE_WIDTH;
  const int ent_wx1 =
      (entrance_offset + 3) * RESTAURANT_TILE_WIDTH; // exclusive
  for (int gy = 0; gy < grid_h; ++gy) {
    const bool bottom_wall =
        (gy + 1) * WALK_CELL_SIZE > (room_size.h + 1) * RESTAURANT_TILE_HEIGHT;
    if (!bottom_wall) {
      continue;
    }
    for (int gx = 0; gx < grid_w; ++gx) {
      const int wx = gx * WALK_CELL_SIZE;
      // Unblock any cell that overlaps the doorway opening.
      if (wx + WALK_CELL_SIZE > ent_wx0 && wx < ent_wx1) {
        blocked[(gy * grid_w) + gx] = false;
      }
    }
  }

  return {.blocked = std::move(blocked), .w = grid_w, .h = grid_h};
}

const RestaurantView::NavGrid& RestaurantView::navigation_grid() const {
  if (!nav_grid_cache_) {
    nav_grid_cache_ = build_navigation_grid();
  }
  return *nav_grid_cache_;
}

RestaurantView::WalkingPath
RestaurantView::compute_walking_path(WorldPoint from, WorldPoint to,
                                     WorldPoint dest_origin,
                                     Size dest_size) const {
  // Copy the cached grid so we can mutate it (unblock the destination
  // chair).
  auto [blocked, grid_w, grid_h] = navigation_grid();

  // Clamp to grid bounds
  const auto clamp_cell = [&](int v, int hi) {
    return std::clamp(v, 0, hi - 1);
  };

  // Unblock the destination chair so DFS can enter from any exposed edge
  const int dest_gx = clamp_cell(to.x / WALK_CELL_SIZE, grid_w);
  const int dest_gy = clamp_cell(to.y / WALK_CELL_SIZE, grid_h);
  {
    const int ux0 = dest_origin.x / WALK_CELL_SIZE;
    const int uy0 = dest_origin.y / WALK_CELL_SIZE;
    const int ux1 = (dest_origin.x + dest_size.w - 1) / WALK_CELL_SIZE;
    const int uy1 = (dest_origin.y + dest_size.h - 1) / WALK_CELL_SIZE;
    for (int y = std::max(0, uy0); y <= std::min(grid_h - 1, uy1); ++y) {
      for (int x = std::max(0, ux0); x <= std::min(grid_w - 1, ux1); ++x) {
        blocked[(y * grid_w) + x] = false;
      }
    }
  }

  const int from_gx = clamp_cell(from.x / WALK_CELL_SIZE, grid_w);
  const int from_gy = clamp_cell(from.y / WALK_CELL_SIZE, grid_h);

  std::vector<WalkingGuest::Orientation> path;
  std::vector<bool> visited(static_cast<size_t>(grid_w * grid_h), false);
  int ops = WALK_MAX_OPS;
  walk_dfs(from_gx, from_gy, dest_gx, dest_gy, grid_w, grid_h, blocked, visited,
           path, ops, 0);
  path = straighten_path(from_gx, from_gy, path, grid_w, grid_h, blocked);
  return {.steps = std::move(path),
          .blocked = std::move(blocked),
          .grid_w = grid_w,
          .grid_h = grid_h};
}

const PlacedFurniture* RestaurantView::get_guest_destination_chair() {
  // Occupied seats occupy the first guests_to_show + guest_occupancy_delta_
  // slots.
  if (shuffled_chairs_.empty()) {
    return nullptr;
  }
  const int first_empty =
      restaurant_->guest_occupancy().guests + guest_occupancy_delta_;
  if (first_empty < 0 ||
      first_empty >= static_cast<int>(shuffled_chairs_.size())) {
    return nullptr;
  }
  return shuffled_chairs_[first_empty];
}

const PlacedFurniture* RestaurantView::find_unserved_guest_chair() {
  for (const auto& [chair_id, ctrl] : guest_controllers_) {
    if (!ctrl.pose().eating) {
      const auto* pf = restaurant_->layout()->find_by_id(chair_id, false);
      if (pf) {
        return pf;
      }
    }
  }
  return nullptr;
}

void RestaurantView::spawn_waiter() {
  const PlacedFurniture* chair = find_unserved_guest_chair();
  if (!chair) {
    return;
  }

  constexpr int tile_w = RESTAURANT_TILE_WIDTH;
  constexpr int tile_h = RESTAURANT_TILE_HEIGHT;

  // +1 skips the top wall tile; KITCHEN_ENTRANCE_ROWS/2 picks the middle row.
  const int entrance_row = restaurant_->layout()->kitchen_entry_offset();
  const int kitchen_y =
      ((entrance_row + 1 + KITCHEN_ENTRANCE_ROWS / 2) * tile_h) + (tile_h / 2);

  constexpr int entry_x = tile_w;
  constexpr int entry_cells = entry_x / WALK_CELL_SIZE;

  constexpr int waiter_bob_index = 12;
  const auto& walk_bob =
      context_.bob->get(compat::format("bob/walk{}.bob", waiter_bob_index));
  // World pos = sprite_center + (5, 8) (from original); at 2x: +10, +16.
  const int blit_offset_x = (walk_bob.width(0) / 2) + 10;
  const int blit_offset_y = (walk_bob.height(0) / 2) + 16;

  const Size chair_sz = bob_renderer_for(*chair).size(*chair);
  const WorldPoint destination{chair->origin().x + (chair_sz.w / 2),
                               chair->origin().y + (chair_sz.h / 2)};
  const core::FurnitureId chair_id = chair->id();

  std::vector<WalkingGuest::Orientation> path(entry_cells,
                                              WalkingGuest::Orientation::E);

  record_walk_debug_destination(destination);
  auto wp = compute_walking_path(WorldPoint{entry_x, kitchen_y}, destination,
                                 chair->origin(), chair_sz);
  if (wp.steps.empty()) {
    // No route: walk out a few steps then immediately return.
    constexpr int turn_steps = 3;
    std::vector<WalkingGuest::Orientation> fail_path(
        entry_cells + turn_steps, WalkingGuest::Orientation::E);
    fail_path.insert(fail_path.end(), entry_cells + turn_steps,
                     WalkingGuest::Orientation::W);

    walking_guest_ = WalkingGuest{
        .position = {0, kitchen_y},
        .kind = WalkingGuest::WalkerKind::WAITER_RETURNING_WITH_PIZZA,
        .guest_type = waiter_bob_index,
        .path = std::move(fail_path),
        .sprite_blit_offset_x = blit_offset_x,
        .sprite_blit_offset_y = blit_offset_y,
    };
    return;
  }
  path.insert(path.end(), wp.steps.begin(), wp.steps.end());

  walking_guest_ = WalkingGuest{
      .position = {0, kitchen_y},
      .kind = WalkingGuest::WalkerKind::WAITER_DELIVERING,
      .destination = destination,
      .destination_chair_id = chair_id,
      .guest_type = waiter_bob_index,
      .path = std::move(path),
      .sprite_blit_offset_x = blit_offset_x,
      .sprite_blit_offset_y = blit_offset_y,
  };
  snapshot_walk_debug_grid(*walking_guest_, wp);
}

void RestaurantView::waiter_delivered() {
  auto& walker = *walking_guest_;

  auto it = guest_controllers_.find(walker.destination_chair_id);
  if (it != guest_controllers_.end()) {
    it->second.set_eating(true);
  }
  mark_dirty();

  constexpr int tile_w = RESTAURANT_TILE_WIDTH;
  constexpr int tile_h = RESTAURANT_TILE_HEIGHT;
  const int entrance_row = restaurant_->layout()->kitchen_entry_offset();
  const int kitchen_y =
      ((entrance_row + 1 + KITCHEN_ENTRANCE_ROWS / 2) * tile_h) + (tile_h / 2);
  constexpr int entry_x = tile_w;
  constexpr int exit_cells = entry_x / WALK_CELL_SIZE;

  // Unblock the served chair so the return path can start from inside it.
  const auto* chair =
      restaurant_->layout()->find_by_id(walker.destination_chair_id, false);
  WorldPoint unblock_origin = walker.destination;
  Size unblock_size{WALK_CELL_SIZE, WALK_CELL_SIZE};
  if (chair) {
    unblock_origin = chair->origin();
    unblock_size = bob_renderer_for(*chair).size(*chair);
  }

  auto wp =
      compute_walking_path(walker.destination, WorldPoint{entry_x, kitchen_y},
                           unblock_origin, unblock_size);

  std::vector<WalkingGuest::Orientation> path = std::move(wp.steps);
  for (int i = 0; i < exit_cells; ++i) {
    path.push_back(WalkingGuest::Orientation::W);
  }

  walker.kind = WalkingGuest::WalkerKind::WAITER_RETURNING;
  walker.path = std::move(path);
  walker.path_index = 0;
  walker.anim_tick = 0;
  snapshot_walk_debug_grid(walker, wp);
}
