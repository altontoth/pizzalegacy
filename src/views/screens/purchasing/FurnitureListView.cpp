/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/purchasing/FurnitureListView.h"
#include <algorithm>
#include <optional>
#include "core/finance/Ledger.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"
#include "engine/ui/InteractionEvent.h"
#include "engine/ui/InteractionManager.h"
#include "factories/FurnitureFactory.h"
#include "factories/SupplierFactory.h"
#include "gfx/Blit.h"
#include "gfx/Draw.h"
#include "magic_enum/magic_enum.hpp"
#include "resources/BobManager.h"
#include "views/screens/StatusDisplay.h"
#include "views/ui/PopupManager.h"
#include "views/ui/finance/FinanceComponents.h"
#include "views/ui/popups/InvoicePopup.h"

namespace {
constexpr int BOX_X = 32;
constexpr int PADDING_TOP = 6;
constexpr int PADDING_BOTTOM = 8;
constexpr int PADDING_LEFT = 6;
constexpr int PADDING_RIGHT = 8;
// extra space outside bg_box
constexpr int BORDER_PADDING = 4;
constexpr int BORDER_WIDTH = 2;
constexpr int SPACING_BETWEEN_ITEMS = 6;
constexpr int MAX_HEIGHT = 284;
constexpr int COLOR_BLACK = 8;
constexpr int GREEN_MARK_BOB_INDEX = 325;
// Text heights for different item types
constexpr int FURNITURE_TEXT_HEIGHT = 24; // Just price line
constexpr int OVEN_TEXT_HEIGHT = 84;      // Price + 3 stat lines
constexpr int STAT_LINE_SPACING = 22;
} // namespace

FurnitureListView::FurnitureListView(GameContext& context, GameState& state,
                                     SupplierID s_id)
    : CategoryOverviewScreen(context, state, SupplierType::FURNITURE),
      supplier_(SupplierFactory::create_supplier(s_id)),
      furniture_bob_(context_.bob->get("bob/moebel.bob")),
      kitchen_bob_(context_.bob->get("bob/kuebobs.bob")),
      green_purchase_mark_(furniture_bob_.frame(GREEN_MARK_BOB_INDEX)) {

  auto raw = FurnitureFactory::all_furniture_by_supplier(s_id);
  furniture_.reserve(raw.size());

  std::ranges::transform(
      raw, std::back_inserter(furniture_), [](const Furniture* f) {
        return FurnitureBatch{
            .furniture = f, .count = 0, .bob_state = std::nullopt};
      });

  int used_height = 0;
  page_starts_.push_back(0);

  for (std::size_t i = 0; i < furniture_.size(); ++i) {
    auto f = furniture_[i].furniture;
    auto& bob_mgr = supplier_type() == SupplierType::KITCHENWARE
                        ? kitchen_bob_
                        : furniture_bob_;
    // Get the bob's height for this furniture item
    int bob_height = bob_mgr.height(f->bobs()[0]);

    int text_height = supplier_type() == SupplierType::KITCHENWARE
                          ? OVEN_TEXT_HEIGHT
                          : FURNITURE_TEXT_HEIGHT;

    // The background box height is the max of bob height and text height
    int content_height = std::max(bob_height, text_height);
    int bg_box_height = content_height + PADDING_TOP + PADDING_BOTTOM;

    // Total item height on screen
    int total_item_height = bg_box_height +
                            (2 * (BORDER_PADDING + BORDER_WIDTH)) +
                            SPACING_BETWEEN_ITEMS;

    if (used_height + total_item_height > MAX_HEIGHT) {
      // This would be too tall, start a new page
      page_starts_.push_back(static_cast<int>(i));
      used_height = 0;
    }
    used_height += total_item_height;
  }

  // mark the end
  page_starts_.push_back(static_cast<int>(furniture_.size()));

  set_up_input_panel();
}

void FurnitureListView::tick() {}

void FurnitureListView::render(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back9"));
  draw_page_header(ctx);
  draw_page_screen(ctx);
}

void FurnitureListView::draw_page_screen(const RenderContext& ctx) {
  draw_furniture_list(ctx);
}

void FurnitureListView::draw_furniture_list(const RenderContext& ctx) {
  interactions_.begin_frame();
  int this_page_start = page_starts_[current_page_];
  int next_page_start = page_starts_[current_page_ + 1];

  int y_pos = 58; // top of where we start drawing

  for (int i = this_page_start; i < next_page_start; ++i) {
    auto ol = furniture_.at(i);
    const auto* f = ol.furniture;
    auto count = ol.count;

    auto& bob_mgr = supplier_type() == SupplierType::KITCHENWARE
                        ? kitchen_bob_
                        : furniture_bob_;
    auto bob = bob_mgr.frame(f->bobs()[0]);

    int text_height = supplier_type() == SupplierType::KITCHENWARE
                          ? OVEN_TEXT_HEIGHT
                          : FURNITURE_TEXT_HEIGHT;

    // For convenience, let's define the background box:
    ViewRect bg_box;
    // We place it around the bob's final position.
    bg_box.x = BOX_X - PADDING_LEFT - BORDER_WIDTH;
    bg_box.y = y_pos - PADDING_TOP - BORDER_WIDTH;
    bg_box.w = bob.width() + (PADDING_LEFT + PADDING_RIGHT);
    // Height is max of bob height and text height
    int content_height = std::max(bob.height(), text_height);
    bg_box.h = content_height + (PADDING_TOP + PADDING_BOTTOM);

    interactions_.create_frame_item(
        ViewRect{bg_box.x, bg_box.y, 360, bg_box.h},
        compat::format("furniture_list.{}.{}",
                       magic_enum::enum_name(supplier_type()), f->name()),
        [this, i](views::ui::InteractionEvent ie) {
          if (ie.type != views::ui::InteractionEvent::Type::ButtonDown) {
            return;
          }
          auto& c = furniture_.at(i).count;
          switch (ie.button) {
          case views::ui::MouseButton::Left:
            if (c == 99) {
              StatusMsg msg{
                  .payload = "You can only order 99 at a time!",
                  .scope = StatusScope::Restaurant,
                  .type = TransientType::Ephemeral,
                  .ttl = std::nullopt,
                  .coalesce_key = "furniture_reposition_close",
              };
              state_.status().post(msg);
            }
            c = std::min(c + 1, 99);
            break;
          case views::ui::MouseButton::Right:
            c = std::max(c - 1, 0);
            break;
          }
          mark_dirty();
        });

    // We also want a "border rectangle" that encloses bg_box:
    int outer_x1 = bg_box.x - BORDER_PADDING - BORDER_WIDTH;
    int outer_y1 = bg_box.y - BORDER_PADDING - BORDER_WIDTH;
    int outer_x2 = bg_box.x + bg_box.w + BORDER_PADDING;
    int outer_y2 = bg_box.y + bg_box.h + BORDER_PADDING;

    // Draw the border:
    uint8_t border_color = 13;
    gfx::draw::box(*ctx.surface, {outer_x1, outer_y1}, {outer_x2, outer_y2},
                   border_color);

    // Fill the bg_box area:
    uint8_t bg_color = 14;
    ctx.surface->fill_rect(bg_box, bg_color);

    ctx.surface->blit_masked(bob, ViewPoint{BOX_X, y_pos});

    // Calculate text offset based on bob width (minimum 154 to handle wide
    // ovens)
    int text_offset = std::max(154, bob.width() + 26);

    // Render furniture name:
    context_.font->render_text_onto(
        *ctx.surface, f->name(),
        ViewPoint{bg_box.x + text_offset, bg_box.y + 2}, FontFace::PIZ_NUMB,
        COLOR_BLACK);

    int info_y = bg_box.y + 24;

    // Render "price:" label
    context_.font->render_text_onto(
        *ctx.surface, "price:", ViewPoint{bg_box.x + text_offset, info_y},
        FontFace::PIZ_NUMB, COLOR_BLACK);

    // Render actual price
    context_.font->render_text_onto(
        *ctx.surface, compat::format("{}", f->price()),
        ViewPoint{bg_box.x + text_offset + 96, info_y}, FontFace::PIZ_NUMB,
        COLOR_BLACK);

    // For ovens, render additional stats
    if (supplier_type() == SupplierType::KITCHENWARE) {
      info_y += STAT_LINE_SPACING;
      context_.font->render_text_onto(
          *ctx.surface, compat::format("Capacity: {} items", f->capacity()),
          ViewPoint{bg_box.x + text_offset, info_y}, FontFace::PIZ_NUMB,
          COLOR_BLACK);

      info_y += STAT_LINE_SPACING;
      context_.font->render_text_onto(
          *ctx.surface, compat::format("baking time: {} min.", f->bake_time()),
          ViewPoint{bg_box.x + text_offset, info_y}, FontFace::PIZ_NUMB,
          COLOR_BLACK);

      info_y += STAT_LINE_SPACING;
      context_.font->render_text_onto(
          *ctx.surface, compat::format("quality: {}", f->quality()),
          ViewPoint{bg_box.x + text_offset, info_y}, FontFace::PIZ_NUMB,
          COLOR_BLACK);
    }

    if (count > 0) {
      ctx.surface->blit_masked(green_purchase_mark_, {300, bg_box.y + 11});
      context_.font->render_text_onto(
          *ctx.surface, std::to_string(count), ViewPoint{-40, bg_box.y + 30},
          FontFace::PIZ_NUMB, COLOR_BLACK, AlignX::Right);
    }
    // Finally, move y_pos down for the next item.  Must match how
    // we computed total_item_height in the constructor:
    int bg_box_height = bg_box.h;
    int total_item_height = bg_box_height +
                            (2 * (BORDER_PADDING + BORDER_WIDTH)) +
                            SPACING_BETWEEN_ITEMS;

    y_pos += total_item_height;
  }
  std::string page_nr = compat::format("- {} -", current_page_ + 1);
  context_.font->render_text_onto(*ctx.surface, page_nr, ViewPoint{0, 316},
                                  FontFace::PIZ_NUMB, COLOR_BLACK,
                                  AlignX::Center);
}

void FurnitureListView::draw_furniture_preview(const RenderContext& ctx,
                                               const Furniture& furniture,
                                               int y) {}
void FurnitureListView::handle_furniture_selection(int furniture_id) {}

void FurnitureListView::show_invoice() {
  using views::ui::finance::InvoiceConfig;
  using views::ui::finance::InvoiceLine;

  InvoiceConfig cfg;
  cfg.subtitle = "for " + state_.active_restaurant()->name();
  cfg.line_font = FontFace::SUPERSMA;
  cfg.line_start_y = 110;
  cfg.line_spacing = 12;
  cfg.label_x = 26;
  cfg.value_x = 278;
  cfg.footer = "thanks for shopping here";

  // Add centered "furniture" label line
  cfg.lines.push_back({"furniture", std::nullopt});

  // We need a spacer to get from the centered label to the line items
  cfg.lines.push_back({"", std::nullopt});

  Money total_cost{0};
  for (const auto& order_line : furniture_) {
    if (order_line.count == 0) {
      continue;
    }
    Money row_cost = order_line.furniture->price() * order_line.count;
    std::string label = order_line.furniture->name();
    cfg.lines.push_back({label, row_cost});
    total_cost += row_cost;
  }

  // Empty line before total
  cfg.lines.push_back({"", std::nullopt});
  cfg.lines.push_back({"", total_cost});

  // Capture by value what we need for the callback
  auto furniture_copy = furniture_;
  const auto* supplier = supplier_;
  cfg.on_dismiss = [this, total_cost, furniture_copy, supplier]() mutable {
    auto* restaurant = state_.active_restaurant();

    if (total_cost.cents > 0) {
      if (!restaurant || !state_.spend_cash(Account::Furniture, total_cost,
                                            restaurant->id())) {
        return;
      }

      for (const auto& order_line : furniture_copy) {
        if (order_line.count > 0) {
          restaurant->purchase_furniture(order_line.furniture,
                                         order_line.count);
        }
      }
    }

    state_.restaurant_placement(std::move(furniture_copy));
    state_.ui().push<RestaurantViewState>(RestaurantViewState{
        .id = state_.active_restaurant()->id(),
        .mode = RestaurantViewMode::PLAYER_NEW_FURNITURE,
        .return_to_supplier_type = supplier->type(),
    });
  };

  context_.popup->origin({74, 40});
  context_.popup->create<InvoicePopup>(std::move(cfg));
}

void FurnitureListView::draw_page_header(const RenderContext& ctx) {
  draw_icon(ctx);
  context_.font->render_text_onto(*ctx.surface, supplier_->name(),
                                  ViewPoint{134, 18}, FontFace::PIZ_NUMB,
                                  COLOR_BLACK);
}

void FurnitureListView::set_up_input_panel() {
  input_panel_->delete_buttons();
  input_panel_->add_button(Icon::ARROW_UP, ButtonType::Horizontal,
                           ButtonPosition::TopLeft, "Turn Page", [this] {
                             // Minus two because we're dealing with the index,
                             // and have one entry more than we need.
                             int max_page_nr =
                                 static_cast<int>(page_starts_.size()) - 2;
                             int last_page_nr = current_page_;
                             current_page_ =
                                 std::clamp(current_page_ + 1, 0, max_page_nr);
                             if (last_page_nr != current_page_) {
                               mark_dirty();
                             }
                           });

  input_panel_->add_button(Icon::ARROW_DOWN, ButtonType::Horizontal,
                           ButtonPosition::MiddleLeft, "Turn Page", [this] {
                             int max_page_nr =
                                 static_cast<int>(page_starts_.size()) - 2;
                             int last_page_nr = current_page_;
                             current_page_ =
                                 std::clamp(current_page_ - 1, 0, max_page_nr);
                             if (last_page_nr != current_page_) {
                               mark_dirty();
                             }
                           });

  input_panel_->add_button(Icon::SHOPPING_CART, ButtonType::Horizontal,
                           ButtonPosition::BottomLeft, "BUY",
                           [this] { show_invoice(); });

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this] { state_.ui().pop(); });
}

SupplierType FurnitureListView::supplier_type() const {
  return supplier_->type();
}
