/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/ScreenText.h"

#include <cstdint>

#include "engine/Size.h"
#include "engine/geom/Spaces.h"
#include "gfx/Surface.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"

namespace views::ui {

namespace {
constexpr int screen_text_margin = 4;
constexpr uint8_t screen_text_color = 15;
} // namespace

void draw_screen_text(FontManager& font, gfx::Surface& surface,
                      std::string_view text, HAlign ax, VAlign ay) {
  const TextStyle style{.color = screen_text_color, .shadow = ShadowStyle{}};
  auto size = font.size_text(text, FontFace::CSTANDAR, style);
  const ViewRect area =
      surface.rect().inset(screen_text_margin, screen_text_margin);
  const ViewPoint pos =
      align_in_rect(area, Size{size.width, size.height}, ax, ay);
  font.render_line_onto(surface, text, pos, FontFace::CSTANDAR, style);
}

} // namespace views::ui
