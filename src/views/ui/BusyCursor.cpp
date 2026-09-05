/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/BusyCursor.h"

#include "engine/Geometry.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/Surface.h"
#include "views/ui/ScreenText.h"

namespace views::ui {

namespace {
constexpr Icon BUSY_ICON = Icon::PADLOCK;
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
    draw_screen_text(*context_.font, surface, label_, HAlign::Left,
                     VAlign::Bottom);
  }
}

} // namespace views::ui
