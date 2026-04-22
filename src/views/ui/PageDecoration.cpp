/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/PageDecoration.h"

#include <array>
#include "engine/Random.h"
#include "gfx/ColorIndex.h"
#include "gfx/Draw.h"
#include "views/base/ViewDefs.h"

namespace views::ui {

void render_paper_background(Surface& surface,
                             const Blitmap<Blitmap2Icon>& blitmap,
                             uint8_t bg_color, bool with_grid) {
  surface.fill(bg_color);

  if (with_grid) {
    constexpr uint8_t GRID_COLOR = 27;
    gfx::draw::grid(surface, {0, 0, surface.width(), surface.height()},
                    GRID_COLOR);
  }
}

void render_corner_decorations(Surface& surface,
                               const Blitmap<Blitmap2Icon>& blitmap,
                               std::optional<ViewRect> box) {
  ViewRect b = box.value_or(ViewRect{0, 0, surface.width(), surface.height()});
  using B = Blitmap2Icon;

  // Corner pieces
  auto tl = blitmap.rect(B::PAGE_LEFT_TOP);
  blitmap.render_icon(B::PAGE_LEFT_TOP, surface, {b.x, b.y});

  auto tr = blitmap.rect(B::PAGE_RIGHT_TOP);
  blitmap.render_icon(B::PAGE_RIGHT_TOP, surface, {b.x + b.w - tr.w, b.y});

  // Bottom corners: 30% chance of alternate variant
  // even though I'm pretty sure the sprites are identical
  auto bl_id = engine::random::rand_percentage_chance(30)
                   ? B::PAGE_LEFT_BOTTOM
                   : B::PAGE_LEFT_BOTTOM_AGAIN;
  auto bl = blitmap.rect(bl_id);
  blitmap.render_icon(bl_id, surface, {b.x, b.y + b.h - bl.h});

  auto br_id = engine::random::rand_percentage_chance(30)
                   ? B::PAGE_RIGHT_BOTTOM
                   : B::PAGE_RIGHT_BOTTOM_AGAIN;
  auto br = blitmap.rect(br_id);
  blitmap.render_icon(br_id, surface, {b.x + b.w - br.w, b.y + b.h - br.h});

  // Edge pieces along the sides, positioned between corners.
  auto ne = blitmap.rect(B::PAGE_NE);
  auto sw = blitmap.rect(B::PAGE_SW);
  auto se = blitmap.rect(B::PAGE_SE);

  blitmap.render_icon(B::PAGE_NW, surface, {b.x, b.y + tl.h});
  blitmap.render_icon(B::PAGE_NE, surface, {b.x + b.w - ne.w, b.y + tl.h});
  blitmap.render_icon(B::PAGE_SW, surface, {b.x, b.y + b.h - bl.h - sw.h});
  blitmap.render_icon(B::PAGE_SE, surface,
                      {b.x + b.w - se.w, b.y + b.h - br.h - se.h});
}

void render_binder_rings(Surface& surface,
                         const Blitmap<Blitmap2Icon>& blitmap) {
  // Get all the binder pieces
  auto binder_start = blitmap.icon_surface(Blitmap2Icon::BINDER_TOP_BEGIN);
  auto binder_middle = blitmap.icon_surface(Blitmap2Icon::BINDER_TOP_MIDDLE);
  auto binder_end = blitmap.icon_surface(Blitmap2Icon::BINDER_TOP_END);

  ViewPoint binder_pos{0, 0};
  constexpr int binder_width_offset = 4;

  // Draw the start piece
  surface.blit_raw(binder_start, binder_pos);
  binder_pos.x += binder_start.width() - binder_width_offset;

  // Calculate where the end piece should go
  int end_x = surface.width() - binder_end.width();

  // Fill the middle section between start and end
  while (binder_pos.x < end_x) {
    if (binder_pos.x + binder_middle.width() <= end_x) {
      surface.blit_raw(binder_middle, binder_pos);
      binder_pos.x += binder_middle.width() - binder_width_offset;
    }
    else {
      // Partial middle piece needed - clip it
      int remaining_width = end_x - binder_pos.x;
      surface.blit(binder_middle,
                   ViewRect{0, 0, remaining_width, binder_middle.height()},
                   binder_pos);
      break;
    }
  }

  // Draw the end piece at the right edge
  surface.blit_raw(binder_end, ViewPoint{end_x, 0});
}

void render_cork_background(Surface& surface, const Surface& permap) {
  // Store rects in an array for easy selection
  std::array<ViewRect, 6> cork_rects = {
      ViewRect{112, 64, 34, 32}, ViewRect{112, 0, 64, 32},
      ViewRect{112, 32, 34, 32}, ViewRect{146, 64, 30, 32},
      ViewRect{150, 32, 26, 32},
  };

  int y = 0;
  constexpr int PANEL_WIDTH = views::base::MAIN_PANEL_WIDTH;
  constexpr int PANEL_HEIGHT = views::base::MAIN_PANEL_HEIGHT;

  while (y < PANEL_HEIGHT) {
    int x = 0;
    while (x < PANEL_WIDTH) {
      // Default to random selection between cork 1 and 2
      ViewRect* area = &cork_rects[1 + engine::random::rand_max_n(2)];

      // Top row always uses 0
      if (y == 0) {
        area = cork_rects.data();
      }

      surface.blit(permap, *area, ViewPoint{x, y});
      x += area->w;
    }

    // Make sure the last column is the right one
    ViewRect final_area = (y == 0) ? cork_rects[3] : cork_rects[4];
    surface.blit(permap, final_area, ViewPoint{PANEL_WIDTH - final_area.w, y});

    y += cork_rects[0].h;
  }
}

void render_push_pin(Surface& surface, const Bob& pins, ViewPoint pos,
                     bool flipped) {
  std::vector<uint8_t> left_pins = {26, 28, 30, 32};
  std::vector<uint8_t> right_pins = {27, 29, 31, 33};

  std::vector<uint8_t>& selected_pins = flipped ? right_pins : left_pins;
  uint8_t pin_id = selected_pins[engine::random::rand_max_n(
      static_cast<int>(selected_pins.size()))];

  auto pin_surface = pins.frame(pin_id);
  surface.blit_masked(pin_surface, pos);
}

void render_decorated_corners(Surface& surface, const Bob& stanbobs,
                              const ViewRect& offsets, int corner_offset,
                              std::optional<gfx::ColorIndex> line_color) {
  const std::array<Surface, 4> corners = {
      stanbobs.frame(35),
      stanbobs.frame(36),
      stanbobs.frame(37),
      stanbobs.frame(38),
  };

  if (line_color) {
    gfx::draw::box(surface, offsets, *line_color, 2);
  }

  const PxOffset corner_delta{corner_offset, corner_offset};
  const ViewPoint top_left = offsets.point() + corner_delta;
  const ViewPoint top_right{
      offsets.point().x + offsets.size().w - corners[1].width() -
          corner_delta.dx,
      offsets.point().y + corner_delta.dy,
  };
  const ViewPoint bottom_left{
      offsets.point().x + corner_delta.dx,
      offsets.point().y + offsets.size().h - corners[3].height() -
          corner_delta.dy,
  };
  const ViewPoint bottom_right{
      offsets.point().x + offsets.size().w - corners[2].width() -
          corner_delta.dx,
      offsets.point().y + offsets.size().h - corners[2].height() -
          corner_delta.dy,
  };

  surface.blit_masked(corners[0], top_left);
  surface.blit_masked(corners[1], top_right);
  surface.blit_masked(corners[3], bottom_left);
  surface.blit_masked(corners[2], bottom_right);
}

} // namespace views::ui
