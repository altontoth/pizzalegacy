/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/util/GameContext.h"
#include "engine/FormatCompat.h"
#include "engine/RenderContext.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionEvent.h"
#include "engine/ui/InteractionLayers.h"
#include "gfx/Surface.h"
#include "resources/BlitmapManager.h"
#include "resources/FontManager.h"
#include "views/helpers/ButtonGeometry.h"
#include "views/screens/InputPanel.h"
#include "views/ui/Icon.h"

#include <algorithm>
#include <functional>
#include <span>
#include <vector>

/*
 * Helper class for rendering paginated supplier lists.
 *
 * Renders a list of suppliers (FoodSupplier, Supplier, etc.) with:
 * - Quality-based icons
 * - Supplier names
 * - Clickable interaction frames
 * - Optional extra information per supplier (the "so far")
 *
 * Template parameter SupplierT must have:
 * - quality() method returning an int
 * - name() method returning a string
 */
template <typename SupplierT> class SupplierListRenderer {
public:
  using ClickCallback = std::function<void(const SupplierT*)>;
  using ExtraInfoRenderer =
      std::function<void(Surface&, const SupplierT*, ViewPoint pos)>;

  SupplierListRenderer(std::vector<const SupplierT*> suppliers,
                       ClickCallback on_click,
                       ExtraInfoRenderer extra_info = nullptr)
      : suppliers_(std::move(suppliers)), on_click_(std::move(on_click)),
        extra_info_(std::move(extra_info)) {}

  void render_page(const RenderContext& ctx, GameContext& game_ctx,
                   views::ui::InteractionLayers& interactions,
                   std::size_t current_page) {
    std::size_t start = current_page * ITEMS_PER_PAGE;
    std::size_t end = std::min(start + ITEMS_PER_PAGE, suppliers_.size());

    ViewPoint pos{30, 51};
    PxOffset banner1_offset{63, 0};
    PxOffset banner2_offset{163, 0};

    std::span<const SupplierT* const> supplier_span(suppliers_.begin() + start,
                                                    suppliers_.begin() + end);
    auto& blitmap1 = game_ctx.blitmap->get<Blitmap1Icon>();

    for (const auto supplier : supplier_span) {
      // Render quality-based icon
      blitmap1.render_icon(
          static_cast<Blitmap1Icon>((supplier->quality() >> 4) - 1),
          *ctx.surface, pos);
      blitmap1.render_icon(Blitmap1Icon::BANNER_FULL, *ctx.surface,
                           pos + banner1_offset);
      blitmap1.render_icon(Blitmap1Icon::BANNER_END_BIT, *ctx.surface,
                           pos + banner2_offset);

      // Create clickable interaction area
      interactions.create_frame_item(
          ViewRect{pos.x, pos.y, 430, 62}, "supplier_" + supplier->name(),
          [this, supplier](views::ui::InteractionEvent ie) {
            if (ie.type == views::ui::InteractionEvent::Type::Click &&
                ie.button == views::ui::MouseButton::Left) {
              on_click_(supplier);
            }
          });

      // Render supplier name
      game_ctx.font->render_text_onto(*ctx.surface, supplier->name(),
                                      ViewPoint{16, pos.y + 20},
                                      FontFace::PIZ_NUMB, 12, AlignX::Center);

      // Render optional extra info (e.g., spending)
      if (extra_info_) {
        extra_info_(*ctx.surface, supplier, pos);
      }

      pos.y += 65;
    }
  }

  void render_page_number(const RenderContext& ctx, GameContext& game_ctx,
                          std::size_t current_page) {
    std::string page_nr = compat::format("- {} -", current_page + 1);
    game_ctx.font->render_text_onto(*ctx.surface, page_nr, ViewPoint{20, 316},
                                    FontFace::PIZ_NUMB, BLACK, AlignX::Center);
  }

  void setup_pagination_buttons(InputPanel* panel, std::size_t& current_page,
                                std::function<void()> mark_dirty_cb,
                                std::function<void()> back_cb) {
    panel->delete_buttons();

    // Up arrow - next page
    panel->add_button(
        Icon::ARROW_UP, ButtonType::Horizontal, ButtonPosition::TopLeft,
        "Turn Page", [this, &current_page, mark_dirty_cb] {
          int max_page_nr = static_cast<int>(max_pages()) - 1;
          std::size_t last_page_nr = current_page;
          current_page =
              std::clamp(static_cast<int>(current_page + 1), 0, max_page_nr);
          if (last_page_nr != current_page) {
            mark_dirty_cb();
          }
        });

    // Down arrow - previous page
    panel->add_button(
        Icon::ARROW_DOWN, ButtonType::Horizontal, ButtonPosition::MiddleLeft,
        "Turn Page", [this, &current_page, mark_dirty_cb] {
          int max_page_nr = static_cast<int>(max_pages()) - 1;
          std::size_t last_page_nr = current_page;
          current_page =
              std::clamp(static_cast<int>(current_page - 1), 0, max_page_nr);
          if (last_page_nr != current_page) {
            mark_dirty_cb();
          }
        });

    // Back button
    panel->add_button(Icon::ARROW_BACK, ButtonType::Square,
                      ButtonPosition::BottomRight, "Back", std::move(back_cb));
  }

  std::size_t max_pages() const {
    return (suppliers_.size() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
  }

  std::size_t supplier_count() const { return suppliers_.size(); }

private:
  static constexpr std::size_t ITEMS_PER_PAGE = 4;

  std::vector<const SupplierT*> suppliers_;
  ClickCallback on_click_;
  ExtraInfoRenderer extra_info_;
};
