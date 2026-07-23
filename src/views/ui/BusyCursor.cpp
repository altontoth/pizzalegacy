/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/BusyCursor.h"

#include "engine/DisplayConstants.h"
#include "engine/geom/Spaces.h"
#include "gfx/Surface.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"

namespace views::ui {

namespace {
constexpr Icon BUSY_ICON = Icon::PADLOCK;
constexpr int caption_margin = 4;
constexpr uint8_t caption_color = 15;
} // namespace

gfx::Surface icon_surface(VgaManager& vga, Icon icon) {
  gfx::Surface out{Size{ICON_WIDTH, ICON_HEIGHT}};
  out.blit(vga.get("gfx/icons"), icon_rect(icon), ViewPoint{0, 0},
           ICON_BACKGROUND);
  // The padlock cell has a stray dot in its top right corner, hidden on a
  // button plate but floating when the icon stands alone.
  if (icon == Icon::PADLOCK) {
    out.fill_rect(ViewRect{ICON_WIDTH - 2, 0, 2, 2}, gfx::TRANSPARENT);
  }
  return out;
}

void draw_activity_caption(FontManager& font, gfx::Surface& surface,
                           std::string_view text) {
  auto size = font.size_text(text, FontFace::CSTANDAR);
  font.render_line_onto(
      surface, text,
      ViewPoint{caption_margin,
                DisplayConstants::SCREEN_HEIGHT - caption_margin - size.height},
      FontFace::CSTANDAR,
      TextStyle{.color = caption_color, .shadow = ShadowStyle{}});
}

void BusyCursor::run(std::function<void()> work, std::string_view label) {
  if (work_) {
    return;
  }
  work_ = std::move(work);
  label_ = label;
  requested_frame_ = context_.clock.frame;
  context_.cursor->custom_cursor(icon_surface(*context_.vga, BUSY_ICON));
}

void BusyCursor::tick() {
  if (!work_ || context_.clock.frame == requested_frame_) {
    return;
  }
  auto work = std::move(work_);
  work_ = nullptr;
  work();
  label_.clear();
  context_.cursor->clear_custom_cursor();
}

void BusyCursor::render_onto(gfx::Surface& surface) const {
  if (work_ && !label_.empty()) {
    draw_activity_caption(*context_.font, surface, label_);
  }
}

} // namespace views::ui
