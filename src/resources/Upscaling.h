/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/gfx/Upscaler.h"

namespace resources {

// How the original pictures and fonts are doubled as they load. Set once,
// before anything loads; what is already loaded keeps its look.
struct Upscaling {
  gfx::Upscaler pictures = gfx::Upscaler::Double;
  gfx::Upscaler text = gfx::Upscaler::Double;
};

void upscaling(Upscaling value);
[[nodiscard]] const Upscaling& upscaling();

} // namespace resources
