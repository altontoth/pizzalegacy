/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/BusyCursor.h"

#include "gfx/Surface.h"

namespace views::ui {

namespace {
constexpr Icon BUSY_ICON = Icon::PADLOCK;
}

gfx::Surface icon_surface(VgaManager& vga, Icon icon) {
  gfx::Surface out{Size{ICON_WIDTH, ICON_HEIGHT}};
  out.blit(vga.get("gfx/icons"), icon_rect(icon), ViewPoint{0, 0},
           ICON_BACKGROUND);
  return out;
}

void BusyCursor::run(std::function<void()> work) {
  if (work_) {
    return;
  }
  work_ = std::move(work);
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
  context_.cursor->clear_custom_cursor();
}

} // namespace views::ui
