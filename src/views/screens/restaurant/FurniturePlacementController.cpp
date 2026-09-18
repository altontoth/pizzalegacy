/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/restaurant/FurniturePlacementController.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

#include "core/character/Player.h"
#include "core/finance/Ledger.h"
#include "core/finance/Money.h"
#include "core/restaurant/Furniture.h"
#include "core/restaurant/PlacedFurniture.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/RestaurantTypes.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/util/GameContext.h"
#include "core/util/LegacyConstants.h"
#include "engine/FormatCompat.h"
#include "engine/Size.h"
#include "engine/ui/ICursorManager.h"
#include "views/base/BobRenderer.h"
#include "views/helpers/ButtonGeometry.h"
#include "views/screens/restaurant/RestaurantView.h"
#include "views/ui/Icon.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/ConfirmPopup.h"

namespace views::screens::restaurant {

namespace {

Rotation rotation_towards(const WorldRect& from, const WorldRect& to) {
  const WorldPoint fc = from.center();
  const WorldPoint tc = to.center();
  const WorldDelta wd = tc - fc;

  if (std::abs(wd.dx) >= std::abs(wd.dy)) {
    return (wd.dx >= 0) ? Rotation::EAST : Rotation::WEST;
  }
  return (wd.dy >= 0) ? Rotation::SOUTH : Rotation::NORTH;
}

} // namespace

FurniturePlacementController::FurniturePlacementController(RestaurantView& view,
                                                           GameContext& context,
                                                           GameState& state)
    : view_(view), context_(context), state_(state),
      furniture_placement_(state.restaurant_placement()) {}

void FurniturePlacementController::prune_empty_batches() {
  std::erase_if(furniture_placement_, [](const FurnitureBatch& b) {
    return b.count <= 0 || b.furniture == nullptr;
  });
}

void FurniturePlacementController::init_active_batch_if_needed() {
  prune_empty_batches();
  if (furniture_placement_.empty()) {
    return;
  }
  set_active_batch(active_batch_index_ ? *active_batch_index_ : 0);
}

FurnitureBatch* FurniturePlacementController::current_batch() {
  if (!active_batch_index_) {
    return nullptr;
  }
  if (*active_batch_index_ >= furniture_placement_.size()) {
    return nullptr;
  }
  return &furniture_placement_[*active_batch_index_];
}

void FurniturePlacementController::set_active_batch(size_t idx) {
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

void FurniturePlacementController::advance_to_next_batch() {
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

void FurniturePlacementController::decrement_or_erase_current_and_maybe_advance(
    bool auto_advance_batches) {
  if (auto* b = current_batch()) {
    if (b->count > 0) {
      --b->count;
    }
    if (b->count == 0) {
      const size_t idx = *active_batch_index_;
      furniture_placement_.erase(furniture_placement_.begin() +
                                 static_cast<ptrdiff_t>(idx));

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

void FurniturePlacementController::throw_away_one_from_current_batch() {
  if (!current_batch()) {
    return;
  }
  decrement_or_erase_current_and_maybe_advance();
  view_.mark_dirty();
}

std::unique_ptr<PlacedFurniture>
FurniturePlacementController::make_cursor_for_batch(const FurnitureBatch& b) {
  if (!b.furniture) {
    return nullptr;
  }

  // An off-map dummy origin; will be re-positioned at click time.
  // Use stored rotation if available (from picked-up furniture)
  const int bob_state = b.bob_state.value_or(0);

  auto pf = std::make_unique<PlacedFurniture>(
      b.furniture, view_.restaurant_->layout()->next_furniture_id(),
      WorldPoint{0, 0}, core::FurnitureId{UNLINKED_FURNITURE}, bob_state);
  return pf;
}

void FurniturePlacementController::pick_up(core::FurnitureId id) {
  auto picked =
      view_.restaurant_->layout()->remove_by_id(id, view_.kitchen_view_);
  assert(picked && "remove_by_id failed");
  assert(picked->id() == id && "picked id changed unexpectedly");
  if (!view_.kitchen_view_) {
    view_.invalidate_nav_grid();
  }

  state_.advance_time_blocks(1);

  FurnitureBatch batch{.furniture = picked->furniture(),
                       .count = 1,
                       .bob_state = picked->bob_state()};
  furniture_placement_.push_back(batch);

  set_active_batch(furniture_placement_.size() - 1);

  update_buttons();
}

WorldRect FurniturePlacementController::candidate_rect(
    WorldPoint p, const PlacedFurniture& ghost) const {
  const auto size = view_.bob_renderer_for(ghost).size(ghost);
  // Furniture top-left should be at the click point
  return WorldRect{p.x, p.y, size.w, size.h};
}

bool FurniturePlacementController::intersects_any_blocking_furniture(
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

  for (const auto& pf : view_.current_furniture_list()) {
    const auto other_size = view_.bob_renderer_for(*pf).size(*pf);
    const WorldRect other{pf->origin(), other_size};

    if (!candidate.intersects(other)) {
      continue;
    }

    const auto* other_f = pf->furniture();
    const auto other_cat = other_f->category();

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

bool FurniturePlacementController::intersects_wall(const WorldRect& r) const {
  const auto room_size = view_.kitchen_view_ ? view_.restaurant_->kitchen_size()
                                             : view_.restaurant_->size();

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

// Only block against other Top-layer DECOR that sits on (intersects) this
// table.
bool FurniturePlacementController::decor_conflicts_on_table(
    const WorldRect& candidate_map, const WorldRect& table_map_rect) const {
  for (const auto& up : view_.current_furniture_list()) {
    const auto* of = up->furniture();
    if (of->category() != FurnitureCategory::DECOR ||
        of->placement_layer() != PlacementLayer::Top) {
      continue;
    }
    const Size other_sz = view_.bob_renderer_for(*up).size(*up);
    const WorldRect other_map{up->origin().x, up->origin().y, other_sz.w,
                              other_sz.h};
    if (!other_map.intersects(table_map_rect)) {
      continue; // decor not on this table
    }
    if (candidate_map.intersects(other_map)) {
      return true; // conflicts with decor already on this table
    }
  }
  return false;
}

void FurniturePlacementController::face_chair_towards_nearest_table_if_any(
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

void FurniturePlacementController::place_cursor_item(WorldPoint p) {
  if (!on_cursor_) {
    return;
  }

  on_cursor_->origin(p);

  if (view_.kitchen_view_) {
    view_.restaurant_->layout()->place_kitchen_furniture(std::move(on_cursor_));
  }
  else {
    view_.restaurant_->layout()->place_furniture(std::move(on_cursor_));
    view_.invalidate_nav_grid();
  }

  state_.advance_time_blocks(1);

  ghost_furniture_topleft_.reset();
  show_furniture_cursor_ = false;

  context_.cursor->clear_custom_cursor();
  on_cursor_.reset();
  view_.mark_dirty();

  // Consume one from the batch of "to be placed" furniture; refresh or advance
  decrement_or_erase_current_and_maybe_advance();

  update_buttons();
}

bool FurniturePlacementController::can_place_now(
    WorldPoint p, WorldRect& out_candidate,
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

void FurniturePlacementController::try_place_at(WorldPoint p) {
  WorldRect cand{};
  const PlacedFurniture* nearest_table = nullptr;
  WorldRect nearest_table_rect{};

  if (!can_place_now(p, cand, nearest_table, nearest_table_rect)) {
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

  place_cursor_item(cand.point());

  // When placing a table, sync chair links: link overlapping chairs and
  // unlink no longer overlapping chairs
  if (placing_table) {
    for (const auto& pf : view_.current_furniture_list()) {
      if (pf->furniture()->category() != FurnitureCategory::CHAIR) {
        continue;
      }
      const Size chair_size = view_.bob_renderer_for(*pf).size(*pf);
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

void FurniturePlacementController::click_with_cursor_on(
    const PlacedFurniture* pf, const WorldRect& furn_world,
    WorldPoint click_world) {
  const auto* curf = on_cursor_->furniture();

  // Special case: placing decor on tables requires clicking inside the table
  if (curf->category() == FurnitureCategory::DECOR &&
      curf->placement_layer() == PlacementLayer::Top &&
      pf->furniture()->category() == FurnitureCategory::TABLE) {

    const WorldRect table_world = furn_world;

    // Center-only: require click within inner area
    constexpr int inner_margin_px = 6;
    const WorldRect inner_world =
        table_world.inset(inner_margin_px, inner_margin_px);

    if (!inner_world.contains(click_world)) {
      view_.handle_furniture_click(pf);
      return;
    }

    // Place at the ghost position, not the click point.
    const Size decor_sz = view_.bob_renderer_for(*on_cursor_).size(*on_cursor_);
    const WorldRect decor_world{ghost_furniture_topleft_->x,
                                ghost_furniture_topleft_->y, decor_sz.w,
                                decor_sz.h};

    // Block only on other Top-layer decor on this same table
    if (decor_conflicts_on_table(decor_world, table_world)) {
      return;
    }

    place_cursor_item(*ghost_furniture_topleft_);
    view_.mark_dirty();
    return;
  }

  // General case: placing any other furniture (chairs, tables, etc.)
  try_place_at(*ghost_furniture_topleft_);
}

bool FurniturePlacementController::rotate_cursor() {
  if (!on_cursor_) {
    return false;
  }
  on_cursor_->rotate();
  // Only update cursor if it's visible (mouse is in viewport)
  if (show_furniture_cursor_) {
    context_.cursor->custom_cursor(
        view_.bob_renderer_for(*on_cursor_).image(*on_cursor_), false);
  }
  view_.mark_dirty();
  return true;
}

void FurniturePlacementController::update_ghost(WorldPoint mouse_world) {
  // Suppress ghost updates while RMB scrolling to prevent the ghost from
  // randomly appearing
  if (view_.rmb_drag_scroller_.is_active()) {
    return;
  }

  // Always update ghost position (clamped to interior bounds - handles
  // sliding)
  ghost_furniture_topleft_ = clamp_furniture_to_interior(mouse_world);

  // If mouse != ghost position, we've clamped, so hide cursor and show ghost
  const bool clamped = (ghost_furniture_topleft_->x != mouse_world.x ||
                        ghost_furniture_topleft_->y != mouse_world.y);

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
          view_.bob_renderer_for(*on_cursor_).image(*on_cursor_), false);
      show_furniture_cursor_ = true;
    }
  }
  view_.mark_dirty();
}

void FurniturePlacementController::hide_cursor() {
  if (show_furniture_cursor_) {
    context_.cursor->clear_custom_cursor();
    show_furniture_cursor_ = false;
  }
}

WorldPoint FurniturePlacementController::clamp_furniture_to_interior(
    WorldPoint mouse_world) const {
  const auto sz = view_.bob_renderer_for(*on_cursor_).size(*on_cursor_);
  const auto room_size = view_.kitchen_view_ ? view_.restaurant_->kitchen_size()
                                             : view_.restaurant_->size();

  constexpr int tile_w = LegacyConstants::RESTAURANT_TILE_WIDTH;
  constexpr int tile_h = LegacyConstants::RESTAURANT_TILE_HEIGHT;

  // Interior bounds in world coordinates
  // Furniture top-left must be at least at (tile_w, tile_h) and at most
  // so that bottom-right doesn't exceed the interior
  const int interior_min_x = tile_w;
  const int interior_min_y = tile_h;
  const int interior_max_x = ((room_size.w + 1) * tile_w) - sz.w;
  const int interior_max_y = ((room_size.h + 1) * tile_h) - sz.h;

  const WorldRect visible = view_.camera_.visible_world_rect();

  // Clamp to the visible portion of the interior
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

void FurniturePlacementController::render_ghost() {
  // Only render ghost when cursor is hidden (outside viewport, frozen state)
  // and not mid-scroll-drag (ghost position is stale while camera moves).
  if (!on_cursor_ || !ghost_furniture_topleft_ || show_furniture_cursor_ ||
      view_.rmb_drag_scroller_.is_active()) {
    return;
  }

  const auto& bob_surface =
      view_.bob_renderer_for(*on_cursor_).image(*on_cursor_);

  ViewPoint dst{ghost_furniture_topleft_->x, ghost_furniture_topleft_->y};
  view_.background_.blit_masked(bob_surface, dst);
}

void FurniturePlacementController::update_buttons() {
  view_.input_panel_->delete_buttons();

  if (on_cursor_ && !return_to_supplier_type_) {
    // Pick up existing furniture mode
    view_.input_panel_->add_button(Icon::CUP, ButtonType::Horizontal,
                                   ButtonPosition::TopLeft, "Throw away",
                                   [this]() { handle_throw_away(); });

    view_.input_panel_->add_button(Icon::TRUCK, ButtonType::Horizontal,
                                   ButtonPosition::MiddleLeft, "Move elsewhere",
                                   [this]() { handle_move_elsewhere(); });

    view_.input_panel_->add_button(Icon::MONEY_BAG, ButtonType::Horizontal,
                                   ButtonPosition::BottomLeft, "Sell it",
                                   [this]() { handle_sell_furniture(); });
  }
  else if (on_cursor_ && return_to_supplier_type_) {
    // Buying new furniture
    view_.input_panel_->add_button(Icon::ARROW_UP, ButtonType::Horizontal,
                                   ButtonPosition::TopLeft, "Next item",
                                   [this]() { handle_next_item(); });

    view_.input_panel_->add_button(Icon::ARROW_DOWN, ButtonType::Horizontal,
                                   ButtonPosition::MiddleLeft, "Previous item",
                                   [this]() { handle_previous_item(); });

    view_.input_panel_->add_button(Icon::CUP, ButtonType::Vertical,
                                   ButtonPosition::TopRight, "Throw away",
                                   [this]() { handle_throw_away(); });
  }
  else {
    // Normal mode: restore default buttons
    view_.set_up_buttons();
  }
}

void FurniturePlacementController::handle_throw_away() {
  if (!on_cursor_) {
    return;
  }

  throw_away_one_from_current_batch();
  update_buttons();
}

void FurniturePlacementController::handle_move_elsewhere() {
  if (!on_cursor_ || !active_batch_index_) {
    return;
  }

  // Add current furniture to the global placement queue for other restaurants
  auto* batch = current_batch();
  if (batch && batch->furniture) {
    FurnitureBatch move_batch{.furniture = batch->furniture,
                              .count = 1,
                              .bob_state = batch->bob_state};
    state_.restaurant_placement().push_back(move_batch);

    // Decrement current batch without auto-advancing
    // (prevents immediately picking up the furniture we just moved)
    decrement_or_erase_current_and_maybe_advance(false);
    update_buttons();
    view_.mark_dirty();
  }
}

void FurniturePlacementController::handle_sell_furniture() {
  if (!on_cursor_) {
    return;
  }

  const auto* furn = on_cursor_->furniture();
  Money sell_price = furn->price() / 2;

  const Account asset = furn->category() == FurnitureCategory::KITCHENWARE
                            ? Account::Machines
                            : Account::Furniture;
  auto do_sell = [this, sell_price, asset]() {
    auto* player = state_.active_player();
    Money full_price = sell_price * 2;
    Money loss = full_price - sell_price;

    // Credit cash with the sell price
    player->financials().transfer(state_.time().current_date(), asset,
                                  Account::Cash, sell_price,
                                  balance_sheet_only);
    // Write off the lost value
    player->financials().transfer(state_.time().current_date(), asset,
                                  Account::ProfitAndLoss, loss,
                                  balance_sheet_only);

    decrement_or_erase_current_and_maybe_advance();
    update_buttons();
    view_.mark_dirty();
  };

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

void FurniturePlacementController::
    check_placement_complete_and_navigate_back() {
  if (furniture_placement_.empty() && return_to_supplier_type_.has_value()) {
    // Replace our own state so that when the user dismisses the purchasing
    // screen, the restaurant view is reconstructed in normal mode with the
    // restaurant re-opened.
    state_.ui().replace_top<RestaurantViewState>(RestaurantViewState{
        .id = view_.restaurant_->id(),
        .mode = RestaurantViewMode::PLAYER_DEFAULT,
        .reopen_restaurant = view_.closed_for_placement_,
    });
    state_.ui().push<CategoryOverviewState>(
        CategoryOverviewState{.type = *return_to_supplier_type_});
  }
}

void FurniturePlacementController::handle_next_item() {
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
      view_.mark_dirty();
      return;
    }
  }
}

void FurniturePlacementController::handle_previous_item() {
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
      view_.mark_dirty();
      return;
    }
  }
}

void FurniturePlacementController::refresh_furniture_cursor_image() {
  if (on_cursor_ && show_furniture_cursor_) {
    context_.cursor->custom_cursor(
        view_.bob_renderer_for(*on_cursor_).image(*on_cursor_), false);
  }
}

} // namespace views::screens::restaurant
