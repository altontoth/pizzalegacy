/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/base/RawRenderer.h"

#include "resources/Upscaling.h"

gfx::Surface RawRenderer::surface(const RawRenderable& r, int scale /*=2*/) {
  return gfx::Surface::from_indexed(r.picture_data(), r.picture_width(),
                                    r.picture_height(),
                                    gfx::PaletteHandle{r.palette_index()},
                                    scale, resources::upscaling().pictures);
}
