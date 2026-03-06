/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/TvPopup.h"

#include <array>
#include <format>

#include "core/restaurant/Restaurant.h"
#include "engine/geom/Spaces.h"
#include "gfx/Draw.h"

static constexpr std::array<const char*, 7> tv_channels = {
    "-------- OFF --------", "Samson Street", "Music Video",
    "Sports Channel",        "Homeshopping",  "CCN",
    "Best of Jimi Swaggert",
};

ViewRect TvPopup::row_rect(int i) { return {46, 44 + (i * 20), 220, 22}; }

TvPopup::TvPopup(const GameContext& ctx, Restaurant& restaurant)
    : BasePopup(ctx), restaurant_(restaurant),
      selected_channel_(restaurant.tv_channel()) {}

void TvPopup::add_interactions(views::ui::InteractionContext ictx,
                               ViewPoint origin) {
  for (int i = 0; i < 7; ++i) {
    ictx.layers.create_frame_item(
        add_origin(row_rect(i), origin), compat::format("tv_row_{}", i),
        [this, i,
         inv = ictx.invalidate](const views::ui::InteractionEvent& ev) {
          switch (ev.type) {
          case views::ui::InteractionEvent::Type::Enter:
            hovered_row_ = i;
            inv();
            break;

          case views::ui::InteractionEvent::Type::Leave:
            if (hovered_row_ == i) {
              hovered_row_ = -1;
              inv();
            }
            break;

          case views::ui::InteractionEvent::Type::Click:
            restaurant_.tv_channel(i);
            finished();
            break;

          default:
            break;
          }
        });
  }
}

void TvPopup::render(Surface& surface, ViewPoint origin) {
  const Size box_size{288, 226};
  const ViewRect outer{origin, box_size};
  surface.fill_rect(outer, 15);
  const int c1 = 17;
  const int c2 = 16;
  const int c3 = 18;
  gfx::draw::box(surface, outer.inset(0, 0), c1, 2);
  gfx::draw::box(surface, outer.inset(2, 2), c2, 2);
  gfx::draw::box(surface, outer.inset(4, 4), c1, 2);
  gfx::draw::box(surface, outer.inset(6, 6), c3, 2);

  const PxOffset title_offset{box_size.w / 2, 18};
  font_->render_line_onto(surface, "TV-Programs", origin + title_offset,
                          FontFace::ASTANDAR, 240, AlignX::CenterPoint);

  for (int i = 0; i < static_cast<int>(tv_channels.size()); ++i) {
    const ViewRect local = row_rect(i);
    const ViewRect screen_row = add_origin(local, origin);
    const bool selected = (i == selected_channel_);
    const bool hovered = (i == hovered_row_);

    if (selected) {
      gfx::draw::box(surface, screen_row, 20, 1);
    }
    if (hovered) {
      surface.fill_rect(screen_row.inset(2, 2), 13);
    }

    const uint8_t text_color = 240;
    font_->render_line_onto(surface, tv_channels[i],
                            origin + PxOffset{local.x + 4, local.y + 4},
                            FontFace::PIZ_NUMB, text_color);
  }

  const PxOffset please_select_offset{50, 200};
  font_->render_line_onto(surface, "Please select...",
                          origin + please_select_offset, FontFace::PIZ_NUMB,
                          14);
}
