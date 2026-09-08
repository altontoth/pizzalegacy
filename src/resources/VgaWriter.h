/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <vector>

#include "engine/gfx/Surface.h"

// Writes the run-length encoding VgaLoader reads.
class VgaWriter {
public:
  // Takes every scale-th pixel, so a surface drawn at the doubled size is
  // written at the original game's size.
  static std::vector<uint8_t> to_bytes(const gfx::Surface& surface,
                                       int scale = gfx::asset_pixel_scale);
};
