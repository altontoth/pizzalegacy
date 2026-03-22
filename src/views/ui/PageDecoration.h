/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gfx/Surface.h"
#include "resources/Blitmap.h"
#include "resources/Bob.h"
#include "resources/FontManager.h"

using gfx::Surface;

namespace views::ui {

void render_paper_background(Surface& surface,
                             const Blitmap<Blitmap2Icon>& blitmap,
                             uint8_t bg_color = 29, bool with_grid = false);

void render_corner_decorations(Surface& surface,
                               const Blitmap<Blitmap2Icon>& blitmap,
                               std::optional<ViewRect> box = std::nullopt);

void render_binder_rings(Surface& surface,
                         const Blitmap<Blitmap2Icon>& blitmap);

void render_cork_background(Surface& surface, const Surface& permap);

void render_push_pin(Surface& surface, const Bob& pins, ViewPoint pos,
                     bool flipped = false);

void render_decorated_corners(
    Surface& surface, const Bob& stanbobs, const ViewRect& offsets,
    int corner_offset,
    std::optional<gfx::ColorIndex> line_color = std::nullopt);
} // namespace views::ui
