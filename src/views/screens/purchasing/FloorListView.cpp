/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/purchasing/FloorListView.h"

#include "core/factories/FloorPatternFactory.h"
#include "core/factories/SupplierFactory.h"
#include "core/restaurant/RestaurantTypes.h"
#include "core/state/GameState.h"
#include "core/util/LegacyConstants.h"
#include "engine/Fatal.h"
#include "engine/FormatCompat.h"
#include "engine/gfx/Blit.h"
#include "magic_enum/magic_enum.hpp"
#include "resources/FontManager.h"

namespace {
constexpr int BOX_X = 32;
constexpr int PREVIEW_W = LegacyConstants::RESTAURANT_TILE_WIDTH * 4;
constexpr int PREVIEW_H = LegacyConstants::RESTAURANT_TILE_HEIGHT * 4;

constexpr int SPACING_BETWEEN_ITEMS = 22;
constexpr int MAX_HEIGHT = 284;

constexpr int COLOR_BLACK = 8;
constexpr int GREEN_MARK_BOB_INDEX = 325;
} // namespace

FloorListView::FloorListView(GameContext& context, GameState& state,
                             SupplierID id)
    : CategoryOverviewScreen(context, state, SupplierType::FLOORING),
      supplier_(SupplierFactory::create_supplier(id)),
      green_purchase_mark_(
          context_.bob->get("bob/moebel.bob").frame(GREEN_MARK_BOB_INDEX)) {

  if (!supplier_) {
    fatal("FloorListView: supplier not found");
  }

  // Pull all floor patterns for this supplier
  const auto& vec = FloorPatternFactory::create(id);
  patterns_ = std::span<const FloorPattern>(vec.data(), vec.size());

  // Pagination layout (based on preview height)
  page_starts_.push_back(0);

  int used_height = 0;
  // Each item has a preview box of PREVIEW_H and that's the total height
  // because there is no padding etc here, just room in between
  const int item_height = PREVIEW_H;

  for (std::size_t i = 0; i < patterns_.size(); ++i) {
    if (used_height + item_height > MAX_HEIGHT) {
      page_starts_.push_back(static_cast<int>(i));
      used_height = 0;
    }
    used_height += item_height;
  }
  page_starts_.push_back(static_cast<int>(patterns_.size()));

  set_up_input_panel();
}

void FloorListView::tick() {}

void FloorListView::charge_exit() { state_.advance_time_blocks(2); }

void FloorListView::back_out() {
  charge_exit();
  state_.ui().pop();
}

void FloorListView::render(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back9"));
  draw_page_header(ctx);
  draw_page_screen(ctx);
}

void FloorListView::draw_page_header(const RenderContext& ctx) {
  context_.font->render_text_onto(*ctx.surface, supplier_->name(),
                                  ViewPoint{150, 18}, FontFace::PIZ_NUMB,
                                  COLOR_BLACK);
}

SupplierType FloorListView::supplier_type() const { return supplier_->type(); }

void FloorListView::draw_page_screen(const RenderContext& ctx) {
  draw_flooring_page(ctx);
}

void FloorListView::draw_flooring_page(const RenderContext& ctx) {
  draw_flooring_list(ctx);
}

void FloorListView::draw_flooring_list(const RenderContext& ctx) {
  interactions_.begin_frame();

  if (patterns_.empty()) {
    context_.font->render_text_onto(
        *ctx.surface, "no patterns", ViewPoint{ctx.surface->width() / 2, 180},
        FontFace::PIZ_NUMB, COLOR_BLACK, TextAnchor::Middle);
    Logger::warn("No patterns available");
    return;
  }

  const int this_page_start = page_starts_[current_page_];
  const int next_page_start = page_starts_[current_page_ + 1];

  int y_pos = 22;

  for (int i = this_page_start; i < next_page_start; ++i) {
    const auto& fp = patterns_[static_cast<std::size_t>(i)];

    const auto preview = create_floor_preview(fp, 4, 4);

    ViewRect clickable_area(BOX_X, y_pos, 340, PREVIEW_H);

    interactions_.create_frame_item(
        clickable_area,
        compat::format("floor_list.{}.{}",
                       magic_enum::enum_name(supplier_type()), i),
        [this, i](views::ui::InteractionEvent ie) {
          if (ie.type != views::ui::InteractionEvent::Type::ButtonDown) {
            return;
          }
          if (ie.button == views::ui::MouseButton::Left) {
            selected_index_.emplace(i);
          }
          else if (ie.button == views::ui::MouseButton::Right) {
            selected_index_.reset();
          }
          mark_dirty();
        });

    ctx.surface->blit_raw(preview, clickable_area.point());

    // Right-side text block
    // leave some room after preview box
    const int text_x = clickable_area.x + PREVIEW_W + 26;
    const int text_y = clickable_area.y + 40;

    auto ppt = supplier_->price_per_tile();
    // Label line: Supplier
    context_.font->render_text_onto(
        *ctx.surface, compat::format("price: {} per tile", ppt),
        ViewPoint{text_x, text_y}, FontFace::PIZ_NUMB, COLOR_BLACK);

    const auto* restaurant = state_.active_restaurant();
    const auto tile_count = restaurant->size().w * restaurant->size().h;
    // Price per tile
    context_.font->render_text_onto(
        *ctx.surface, compat::format("* {} = {}", tile_count, tile_count * ppt),
        ViewPoint{text_x, text_y + 22}, FontFace::PIZ_NUMB, COLOR_BLACK);

    // Selection mark
    if (selected_index_ && *selected_index_ == i) {
      ctx.surface->blit_masked(green_purchase_mark_,
                               {300, clickable_area.y + 11});
    }

    // Advance Y
    const int clickable_area_height = clickable_area.h;
    const int total_item_height = clickable_area_height + SPACING_BETWEEN_ITEMS;
    y_pos += total_item_height;
  }

  // Page number
  const std::string page_nr = compat::format("{}", current_page_ + 1);
  context_.font->render_text_onto(
      *ctx.surface, page_nr, ViewPoint{(ctx.surface->width() / 2) + 150, 316},
      FontFace::PIZ_NUMB, COLOR_BLACK, TextAnchor::Middle);
}

void FloorListView::set_up_input_panel() {
  input_panel_->delete_buttons();

  // Page down
  input_panel_->add_button(Icon::ARROW_UP, ButtonType::Horizontal,
                           ButtonPosition::TopLeft, "Turn Page", [this] {
                             const int max_page =
                                 static_cast<int>(page_starts_.size()) - 2;
                             const int prev = current_page_;
                             current_page_ =
                                 std::clamp(current_page_ + 1, 0, max_page);
                             if (prev != current_page_) {
                               mark_dirty();
                             }
                           });

  // Page up
  input_panel_->add_button(Icon::ARROW_DOWN, ButtonType::Horizontal,
                           ButtonPosition::MiddleLeft, "Turn Page", [this] {
                             const int max_page =
                                 static_cast<int>(page_starts_.size()) - 2;
                             const int prev = current_page_;
                             current_page_ =
                                 std::clamp(current_page_ - 1, 0, max_page);
                             if (prev != current_page_) {
                               mark_dirty();
                             }
                           });

  // BUY / confirm
  input_panel_->add_button(
      Icon::SHOPPING_CART, ButtonType::Horizontal, ButtonPosition::BottomLeft,
      "BUY", [this] {
        if (!selected_index_) {
          alert_no_selection();
          return;
        }
        auto* player = state_.active_player();
        auto* restaurant = state_.active_restaurant();
        const auto tile_count = restaurant->size().w * restaurant->size().h;
        const auto price = tile_count * supplier_->price_per_tile();
        // Price per tile
        if (player->spend_cash(state_.time().current_date(), Account::Furniture,
                               price, restaurant->id())) {
          restaurant->layout()->floor_pattern(patterns_[*selected_index_]);
          charge_exit();
          state_.ui().push<RestaurantViewState>(RestaurantViewState{
              .id = state_.active_restaurant()->id(),
              .mode = RestaurantViewMode::PLAYER_DEFAULT,
          });
        }
      });

  // Back
  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this] { back_out(); });
}
