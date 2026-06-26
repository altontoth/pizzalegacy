/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/geom/Spaces.h"

namespace gfx {
class Surface;
}
class RawRenderable;

namespace views::helpers {

void draw_framed_portrait(gfx::Surface& dst, const RawRenderable& portrait,
                          ViewPoint pos);

} // namespace views::helpers
