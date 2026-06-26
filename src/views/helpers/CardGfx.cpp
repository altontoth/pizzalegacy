/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/helpers/CardGfx.h"

#include <cstdint>

#include "gfx/Surface.h"
#include "views/base/RawRenderer.h"

namespace views::helpers {

void draw_framed_portrait(gfx::Surface& dst, const RawRenderable& portrait,
                          ViewPoint pos) {
  RawRenderer r;
  auto picture = r.surface(portrait);

  constexpr int frame_w = 118;
  constexpr int frame_h = 132;
  constexpr uint8_t photo_light_bg = 12;
  constexpr uint8_t photo_darker_bg = 11;
  constexpr uint8_t photo_white_bg = 15;

  auto draw_layer = [&dst, pos](int dx, int dy, uint8_t color) {
    dst.fill_rect(ViewRect{pos.x + dx, pos.y + dy, frame_w, frame_h}, color);
  };
  // Back to front to fake a stacked frame.
  draw_layer(0, 0, photo_light_bg);
  draw_layer(2, -2, photo_darker_bg);
  draw_layer(4, -4, photo_white_bg);

  dst.blit_raw(picture, ViewPoint{pos.x + 11, pos.y + 3});
}

} // namespace views::helpers
